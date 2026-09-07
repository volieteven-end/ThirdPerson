# 剩余 P0 + 三项 P1：C++ 修复与 UE 配置

项目：`E:\UNREAL\ue projects\ThirdPerson`。2026-09-06。

## 这次具体改了什么

| 问题 | C++ 行为 | 你在 UE 中完成的部分 |
|---|---|---|
| P0：攻击、闪避还能接收 WASD | `IsMovementInputLocked()` 统一覆盖死亡、受击、格挡、近战／远程攻击、闪避；进入地面动作清掉残余行走速度，但保留根运动 | 检查输入走原来的 Input Action；四方向闪避仍填根运动 Montage |
| P1：连段必须等长窗口结束 | 一次按下缓存一段；只有独立 `Combo Chain Point` 才允许衔接；结束／受击／死亡清缓存；最后一段不自动绕回第一段 | 在前 3 段攻击 Montage 添加新的 C++ Notify，调整其时间 |
| P1：所有空中攻击都下砸 | 空中左键走 AirLight，保留物理惯性与重力；另一个 Input Action 走 AirDive；下砸在 `Landed` 后进落地段 | 配置空中普攻数组、下砸三段 Montage 和 `IA_AirDive` |
| P1：转身末尾额外旋转 | 四个根运动 Montage 驱动胶囊旋转；旧 `CommitTurnInPlace()` 不再加 90/180°；移动／攻击／受击可打断转身 | 建 4 个转身 Montage、启用源 Sequence 根运动、退出旧 Sequence 转身路径 |

附带修正：`CheckpointActor.cpp` 补上 `ItemDefinition.h`，解决本次基线完整编译发现的 C2679；`StopSprint()` 不再无条件将你配置的移动速度改回 450，`Sprint Max Walk Speed` 可在角色默认值填写。

**这次修改的是 C++，未保存或改写 Content 里的 .uasset。** 现有 Skeleton、Warp Target Name、BS 和 Rate Scale 配置保持原样。下面的动画资产接线仍要在 UE 中完成。

## 0. 先让新 C++ 类进入编辑器

1. 保存正在编辑的蓝图／动画，关闭 UE 编辑器。
2. 右键 `E:\UNREAL\ue projects\ThirdPerson\ThirdPerson.uproject` → Generate Visual Studio project files（刷新工程文件，使新 Notify 与 Tests 文件可见）。
3. 在 IDE 选择 **Development Editor / Win64**，生成 `ThirdPerson`；也可在 PowerShell 执行：

```powershell
& 'D:\Unreal5.8\UE_5.8\Engine\Build\BatchFiles\Build.bat' ThirdPersonEditor Win64 Development '-Project=E:\UNREAL\ue projects\ThirdPerson\ThirdPerson.uproject' -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles
```

4. 重新打开项目，再编译 `BP_TPCCharacter` 和 `ABP_TPCCharacter`。

新增了 UCLASS 和 UPROPERTY，这一次采用关闭编辑器后的完整生成，而不是只按 Live Coding。已验证的编译使用同源码的独立候选工程；运行中的编辑器 DLL 保持原样。

## 1. 共同的 AnimGraph 设置

打开 `/Game/Third/Character/ABP_TPCCharacter`：

```text
Locomotion 状态机
    → Slot（DefaultGroup.DefaultSlot）
    → Inertialization
    → 现有 IK／后处理（如果原来就有）
    → Output Pose
```

- Class Defaults → Root Motion Mode：**Root Motion from Montages Only**。
- Slot 节点启用 **Always Update Source Pose**，让底下的状态机在播放 Montage 时继续更新。
- 本文新建的所有 Montage 都使用 **DefaultGroup.DefaultSlot**，与现有攻击槽一致。
- 若已放了 Inertialization 且涵盖这条输出路径，就保留一个即可。
- 每个状态出口都要有有效 Pose；旧转身 Enum Blend 的 Default Pose 若仍保留，应连接 Idle，而非留空。

Montage 的 Slot 必须接入 AnimGraph 才会参与最终姿态；根运动开关分别涉及源动画和 AnimBP。参见 [Epic：Using Animation Montages](https://dev.epicgames.com/documentation/unreal-engine/using-animation-montages?application_version=4.27) 与 [Epic：Root Motion](https://dev.epicgames.com/documentation/en-us/unreal-engine/root-motion-in-unreal-engine)。

## 2. 攻击／闪避锁移动（P0）

### 角色输入

- 保留原来的 `IA_Move`、`IA_Attack`、`IA_Dash`、`IA_Block` 等，按键继续在 `IMC_Default` 配。
- C++ 绑定的是 Input Action 事件，没有绑定 W/A/S/D、Alt、鼠标等实体按键。
- 检查 `BP_TPCCharacter` 中是否还存在旧的直接 `Add Movement Input`、`Launch Character`、`Play Montage` 攻击／闪避流程。让这些操作统一走原有 C++ 输入入口，避免同时再执行一份蓝图攻击逻辑。
- 左键建议保持一次按下触发 `Started`，不使用每帧 `Triggered` 重复调用攻击。

### 闪避

在 `BP_TPCCharacter → Class Defaults → Animation → Dash` 填现有四个：

| 字段 | 资源 |
|---|---|
| Dash Forward Montage | `AM_Dodge_F_Anim_Montage` |
| Dash Backward Montage | `AM_Dodge_B_Anim_Montage` |
| Dash Left Montage | `AM_Dodge_L_Anim_Montage` |
| Dash Right Montage | `AM_Dodge_R_Anim_Montage` |

源 Sequence 启用根运动，Montage 使用 DefaultSlot。代码按当前 WASD 的世界方向与角色前／右向量选择最近的四方向；斜向输入会归入其中一个方向。没有按方向键时用角色前方。

这条路径只用根运动位移，不再额外 LaunchCharacter。资源缺失或播放失败时不扣体力、不启动冷却、不留下移动锁。原 `Dash Strength` 作为兼容属性保留，根运动闪避的距离由动画决定。

**锁的是输入，不是冻结胶囊**：挥剑／闪避动画中的根位移，以及已有 Motion Warping，仍会正常推动角色。空中输入锁也不会将当前惯性清零。

若你刚重新配置过 BS 速度：行走／跑步基础速度在 CharacterMovement 配；冲刺速度另设 `BP_TPCCharacter → Movement → Sprint Max Walk Speed`。本次没有替你重设 BS 样本坐标。

## 3. 独立连段衔接点（P1）

### CombatComponent 参数

`BP_TPCCharacter → Components → CombatComponent → Combat → Combo`：

```text
Allow Early Combo Buffer = true
Chain On Legacy Combo Window End = false
```

第二项仅用于临时兼容旧窗口；保持 false 才是本次的新衔接机制。

### Montage 时间轴

依次打开 `AM_Sword_Attack_01 / 02 / 03`：

1. 在 **Notifies 轨道的空白处**右键 → Add Notify → **Combo Chain Point**。
2. 选择这次 C++ 新增的 Notify 类；不是 New Notify 后手写同名 Skeleton Notify，也不是 Notify State。
3. 将它放到主挥击完成、可以接下一刀的位置。新 Notify 的 **Montage Tick Type 保持 Queued**。
4. 保留原来的 `Damage`、`WeaponEffect` 等窗口。新的 Chain Point 不负责伤害判定。
5. 旧 `ComboInput` Notify State 可以保留用于限制输入窗口（当 Early Buffer 关闭时），但它的结束默认不再衔接。
6. 第四段暂不加 Chain Point，作为连段收尾。

以当前已审计的伤害窗口为参照，下面只是**第一轮调手感起点**，并非已经逐帧确认的最终值：

| Montage | 旧伤害窗口约 | Chain Point 起始建议 |
|---|---|---|
| Attack 01 | 0.214～0.356 s | 0.55 s 左右 |
| Attack 02 | 0.223～0.408 s | 0.55 s 左右 |
| Attack 03 | 0.332～0.502 s | 0.68 s 左右 |

如果你已修改了动画或播放倍率，以当前时间轴和挥剑落点为准：从伤害窗口末尾往后留少量恢复动作，再调整。接得太急就后移；拖沓就前移。不要把 Chain Point 放进仍在命中的主要伤害段内，也不要与 Damage 窗口结束放在完全同一帧。

带剑时，攻击数组优先取武器数据资产中的 Montage（例如 `DA_TestSword`），并非始终取角色 CombatComponent 上的默认空手数组。确认你编辑的是武器实际使用的那四段。

### 实际输入行为

```text
按一下左键 → 第一段
第一段尚未到 Chain Point 时再按 → 缓存一次，不立刻切动画
到 Chain Point → 消耗缓存，开始第二段
到过 Chain Point 后才按 → 在本段尚未结束时也能衔接
一直连点 → 每一段最多缓存一个后续动作
受击／死亡／被其他动作打断 → 清空缓存与伤害窗口
```

自然播放结束不会再偷偷开启下一段。未配置 Chain Point 时，即使缓存了输入，也会正常收招结束。

Sequence 本身的 Notify 仍可在 Montage 播放时触发；同一动作不要在 Sequence 和 Montage 各放一份相同衔接点。本次还按 Montage 实例 ID 过滤了上一段延迟到达的通知，避免它关闭下一段的伤害／武器特效。Notify 的一般编辑方法见 [Epic：Animation Notifies](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-notifies-in-unreal-engine)。

## 4. 空中普攻与下砸分开（P1）

### A. 空中左键：AirLight

建议用文件夹中的 `Air_Attack_01_Anim`、`Air_Attack_02_Anim` 创建两个 Montage，例如：

```text
AM_Sword_AirLight_01
AM_Sword_AirLight_02
```

在 `BP_TPCCharacter → CombatComponent → Combat → Air Attacks → Air Attack Montages` 按顺序填入。

- 数组留空时，兼容旧 `Air Attack Montage`，即原来的 `AM_Sword_AirAttack`，只打一段。
- 多段时在第一段加 `Combo Chain Point`；每段各自配置一次 Damage 窗口。
- 每次腾空最多使用数组内的段数，落地后恢复；受击／取消不会在半空无限刷新次数。
- **本版 AirLight 采用物理运动，不悬停**：重力照常生效，横向惯性保留，落地时结束空中普攻并回到地面姿态。
- 若要严格保留 XY 惯性，请为 AirLight 使用专用的 Sequence 副本，设置 **Enable Root Motion = false、Force Root Lock = true**，不添加平移 Motion Warping。否则动画自身的根运动仍可能接管 XY 位移。
- Montage Blend In／Out 可先用 0.08／0.12 s，再按动画调整。

### B. 独立下砸：AirDive

1. 新建 `IA_AirDive`：Value Type = Boolean。
2. 在 `IMC_Default` 将它映射到你选择的键，例如 **Q**（这是建议，代码没有写死 Q）。
3. `BP_TPCCharacter → Class Defaults → Input → Air Dive Action` 指向 `IA_AirDive`。
4. 用 `Air_Attack_05_In_Anim / Air_Attack_05_Loop_Anim / Air_Attack_05_Out_Anim` 创建一个 Montage，例如 `AM_Sword_AirDive`。
5. 使用专用 Sequence 副本：Enable Root Motion = false，Force Root Lock = true，让 CharacterMovement 的下落／碰撞负责位移。
6. 在同一 DefaultSlot 时间轴依次排三段，创建三个 **Montage Section**，名称精确为：

```text
Start  →  Loop  ↺（持续到真实落地）
             Land → 结束
```

`Start` 从 In 开始，`Loop` 从 Loop 动画开始，`Land` 从落地恢复片段开始。代码会设置 Start→Loop、Loop→Loop、Land→None；实际碰撞落地时才跳到 Land。

**Out 动画的裁切很重要**：这个资源开头仍含空中的姿势变化。把 Land 对应动画片段的起点裁到脚／身体真正接触地面的恢复部分，别在已经物理落地后又从空中倒立段重播。

然后在 `CombatComponent → Combat → Air Attacks` 填：

```text
Air Dive Montage = AM_Sword_AirDive
Air Dive Start Section = Start
Air Dive Loop Section = Loop
Air Dive Land Section = Land
Air Dive Damage Multiplier = 1.75（初始值，可调）
```

下坠速度仍使用兼容字段 `Combat → Special Attacks → Air Attack Downward Velocity`，默认 650；**现在只作用于 AirDive**。空中左键不再使用它。

第一版建议只在 Land 的落地挥击阶段加一次 Damage 窗口，Loop 不加循环伤害窗口，避免每圈重置命中列表导致多次伤害。AirDive 不放 Combo Chain Point。默认不允许从正在播放的攻击强行切入下砸，需要前一动作结束；取消窗口可作为后续独立设计。

## 5. 根运动转身（P1）

源资源位于 `/Game/Third/SwordAnimation`：

| Sequence | 建议新 Montage 名 | 角色默认值字段 |
|---|---|---|
| Turn_L_90_Anim | AM_Turn_L_90 | Turn Left 90 Montage |
| Turn_R_90_Anim | AM_Turn_R_90 | Turn Right 90 Montage |
| Turn_L_180_Anim | AM_Turn_L_180 | Turn Left 180 Montage |
| Turn_R_180_Anim | AM_Turn_R_180 | Turn Right 180 Montage |

1. 这四个 Sequence 本身有旋转根轨迹；在源动画（或专用副本）启用 **Enable Root Motion**，Root Motion Root Lock 可选 **Anim First Frame**，Force Root Lock 保持启用。
2. 创建上表四个 Montage，DefaultSlot，单次播放，不设循环 Section；保留 Enable Auto Blend Out。
3. 在 `BP_TPCCharacter → Class Defaults → Animation → Root Motion Turn` 填四个 Montage，`Enable Root Motion Turn = true`。
4. 新转身路径不需要 `CommitTurn`／`TurnFinished` 通知来旋转胶囊。可以移除新 Sequence 副本中的旧 `CommitTurn`；兼容函数已取消额外旋转，但也不要在 BP 的通知事件后再接 `Add Actor Rotation` 或瞬间 `Set Actor Rotation`。
5. **关闭旧 Ground→Turn Sequence 状态路径**。地面状态仍输出站立 BS，转身由状态机后的 Slot 叠上去。旧 Turn 状态若暂时保留，确保它可回 Ground；最直接的是断开旧进入路径。

原 `ABP_TPCCharacter → Class Defaults → Turn In Place` 的阈值继续有效：90° 阈值初始 55°、180° 阈值初始 135°、结束间隔初始 0.12 s。

触发条件：自由视角、地面静止、未蹲下／格挡／受击／攻击／闪避。锁定敌人时沿用锁定转向，不重复触发站立转身 Montage。

旋转以动画实际输出为准，**不是最后一帧再强行对齐任意视角角度**。例如视角偏差 110°，播放 90° 后允许留下约 20° 的偏差；这能避免末帧跳转。移动、跳跃、攻击、格挡可打断转身；受击和死亡会清理动作状态。锁定模式恢复时由 CharacterMovement 的 Rotation Rate 平滑追向控制器，不再直接将 Controller Yaw 瞬间赋给 Actor。

## 6. PIE 验收清单

- [ ] 按住 W 再攻击：非根运动动作不继续行走；根运动位移仍存在。
- [ ] 闪避时连续按 WASD／左键：不叠加走路或另一攻击；闪避结束恢复控制。
- [ ] 第一刀很早再按一次：先保持第一刀，到 Chain Point 才接第二刀；不按则只打一刀。
- [ ] 第四刀结束：不会自动绕回第一刀；受击后没有残留的自动出刀。
- [ ] 空中左键：不突然向下加速；空中右键：保留原先横向惯性。
- [ ] 下砸：空中停在 Loop，真实落地后播放 Land；Land 不从空中倒立帧重播。
- [ ] 站立转身：胶囊跟随动画连续旋转，结束无额外 90/180° 跳变；途中移动和受击可退出。
- [ ] 死亡：攻击、缓存、伤害窗口停止；末尾通知不能把死亡角色解锁。
- [ ] 运行 `Automation RunTests ThirdPerson.ActionSystem`：5 组 C++ 状态回归测试通过。

自动化测试使用临时世界，覆盖 C++ 状态和惯性逻辑；上述资源的实际播放、碰撞落地、Slot 混合、脚底与手感需要配置后在 PIE 按清单确认。

## 文件入口

- [角色动作与输入](E:/UNREAL/ue%20projects/ThirdPerson/Source/ThirdPerson/Character/TPCCharacter.cpp)
- [连段／空中攻击](E:/UNREAL/ue%20projects/ThirdPerson/Source/ThirdPerson/Components/CombatComponent.cpp)
- [新连段 Notify](E:/UNREAL/ue%20projects/ThirdPerson/Source/ThirdPerson/Animation/ComboChainPointNotify.cpp)
- [动画实例兼容迁移](E:/UNREAL/ue%20projects/ThirdPerson/Source/ThirdPerson/Animation/TPCAnimInstance.cpp)
- [C++ 自动化测试](E:/UNREAL/ue%20projects/ThirdPerson/Source/ThirdPerson/Tests/ActionSystemTests.cpp)
- [完整编译／测试／回退记录](E:/UNREAL/ue%20projects/ThirdPerson/Saved/ActionSystemFix/20260906_1420/VERIFICATION.txt)

回退包位于 `E:\UNREAL\ue projects\ThirdPerson\Saved\ActionSystemFix\20260906_1420`；只恢复本次变更的 Source 文件，不改动你之后在 UE 中保存的动画、蓝图或按键资产。回退之后也需要完整重新编译 C++。
