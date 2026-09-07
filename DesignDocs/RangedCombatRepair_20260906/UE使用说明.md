# 远程敌人、弓箭和锁定 Alt 闪避修复

更新日期：2026-09-06。正式项目：`E:/UNREAL/ue projects/ThirdPerson/ThirdPerson.uproject`。

## 本次已经应用的修改

### 1. 锁定敌人，攻击中按 Alt 闪避
- 确认按键为 **Alt 闪避**，Shift 仍是快跑；没有更改输入映射。
- 运行时原来同时存在 `MotionWarpingComponent`（C++）和 `MotionWarping`（蓝图）。UE 的 CharacterMovement 根运动预处理委托为单播；后初始化的蓝图组件覆盖了原组件的绑定。C++ 更新目标的组件与实际处理根运动的组件不一致。
- 已删除 `BP_TPCCharacter` 中重复添加的蓝图组件，保留原生组件。修复前原生组件在锁定攻击测试中没有 modifier，修复后观测到实际攻击 Warp 窗口。
- `Dash()` 通过消耗、冷却、动画有效性检查后，先取消攻击，再调用 `ClearAttackRootMotionForDodge()`：停攻击吸附、禁用旧 Warp modifier、移除 `AttackTarget`、清理上一动作已提取的根位移；最后启动闪避蒙太奇。没有传送角色，也没有重置相机来掩盖问题。
- 保留四向根运动闪避。保留第一次闪避结束后 **0.65 秒**的连招衔接，闪避中预输入一次左键会在结束后接下一段；超时或第二次闪避按原规则重开连招。
- 同时补齐 Shift 的 Canceled 事件和攻击/闪避锁输入期间的快跑状态清理。

**验证边界：** 在无渲染 PIE 中，驱动实际输入处理函数，测试锁定攻击后 0.05 / 0.20 / 0.50 / 0.85 秒按 Alt，覆盖四方向，共 16 组、640 次采样；显式启用骨骼刷新。角色根骨与胶囊脚底偏差为 2 cm，视角目标始终是主角，镜头到胶囊距离最大约 407.92 cm，各组均成功闪避并接续攻击。原来“角色飞走但镜头不动”的偶发现象在修复前后这些测试中均未复现；本次结论是重复绑定和动作切换清理问题已修复；原偶发问题仍待实际场景复测。

### 2. 远程敌人行为树
资源：`/Game/Third/AI/BT_RangeEnemy`，黑板：`/Game/Third/AI/BB_RangedEnemy`。

原来 Investigate / Patrol 分支没有接入根 Selector；巡逻任务的 Key 为空，Move To 又在使用 TargetActor。现在已经连回主树并新增 Vector `PatrolLocation`。

根分支优先级：
`Dead → Stunned → SeparateFromAllies → HasTarget → Investigate → Patrol`

- 没有目标：使用关卡实例的 `Patrol Points`，或在 `HomeLocation` 周围随机选择完整可达的导航点。
- 有目标：持续 Gameplay Focus 指向玩家，关闭 Orient Rotation to Movement，启用 Use Controller Desired Rotation；Move To 启用 Allow Strafe。后退移动不再使敌人转身背对玩家。
- 小于 450 cm 开始后退，退到至少 550 cm 才退出后退状态，减少边界反复切换；有效射击距离为 450–950 cm，并检查视线。
- 追逐的停止距离为 300 cm，消除旧 350–450 cm 距离段进退都不执行的问题。
- 后退点检测导航和完整路径，尝试侧向或缩短距离；死角中保持面向玩家等待重试，不转而向玩家走。
- 中止射击任务时取消待发攻击；停止行为树或失去目标后恢复原朝向设置。

### 3. 弓箭伤害、停留与对象池
- 原 `BP_ArrowProjectile` 的根 CollisionSphere 是 NoCollision，而 ArrowMesh 是 BlockAllDynamic。伤害回调只绑定在球上，所以模型推人却不触发伤害。
- 现在由球体进行 QueryOnly 扫掠命中；模型部件关闭碰撞和物理模拟。构造完成后、每次复用时都会重新建立这一规则。
- 实际射击使用武器有效伤害，经过 Health / Combat 的现有防御流程；友方敌人不受敌方箭伤害。无防御测试中 10 点箭伤令 HP 从 100 变成 90。
- 命中后只结算一次，箭停止运动和碰撞：命中角色时挂到最近骨骼，命中场景时挂到命中的组件。
- **5 秒从命中时开始计时**，不是从发射时开始。落地和扎在主角身上都适用。持续飞行没有命中的箭默认 10 秒回收。
- `UProjectilePoolSubsystem` 按世界管理对象池；按具体箭蓝图类复用。回收会清除计时器、挂接、速度、伤害、Owner / Instigator 等；复用时重新指定 UpdatedComponent。
- 默认每种箭最多 128 个，总池上限 512。池满时跳过本次生成，不提前回收仍在飞行或尚未满 5 秒的箭。

## 在 UE 里怎么用

1. 重新打开上面的正式项目。源码和 4 个蓝图/行为树/黑板资源已应用，项目已经编译；不需要手动重新接行为树。
2. `BP_TPCCharacter`：保留继承的 `MotionWarpingComponent`，不要再 Add 一个同类型组件。`ABP_TPCCharacter` 仍为 **Root Motion from Montages Only**。攻击通知的 Warp Target Name 保持 `AttackTarget`，闪避使用原来的四个根运动蒙太奇。
3. 选中关卡中的 `BP_EnemyRangedCharacter`：
   - 指定巡逻路线：放几个 TargetPoint，在该实例 `AI → Patrol Points` 中按顺序添加。
   - 随机区域巡逻：Patrol Points 留空，设置 `AI → Patrol → Ranged Patrol Radius`，默认 **800 cm**。区域围绕出生时记录的 HomeLocation；这个半径只限制无目标巡逻，不是战斗追击边界。
   - 巡逻区域应由 Nav Mesh Bounds Volume 覆盖。新增地图需要构建导航；本次测试的 `Lvl_ThirdPerson` 已有可达路径。
4. 调整远程距离：打开 `BT_RangeEnemy`，选择根上的 **Update Ranged Combat Context** Service，修改 Minimum / Maximum Attack Distance、Retreat Trigger / Stop Distance。建议先使用 450 / 950 / 450 / 550。
5. `BP_ArrowProjectile → Class Defaults`：
   - Projectile / Lifetime → Impact Lifetime = **5**。
   - Flight Lifetime = **10**。
   - Projectile / Pool → Max Pooled Instances = **128**。
6. 箭伤害在 `DA_TestBow` 的 Damage 中调整，当前是 **10**；其 Projectile Class 保持 BP_ArrowProjectile。角色格挡、无敌帧仍会影响最终掉血。

## 验证与回滚

- C++ 自动化：修改版及正式项目均 **13/13**，其中包含根运动交接和箭重复命中/对象池复用检查。
- 测试副本无渲染 PIE：远程与箭 **19/19**、原连招和近战移动回归 **13/13**、锁定 Alt 汇总检查 **8/8**。
- 正式项目：6 个关键资源加载/蓝图编译成功；PIE 确认只有 1 个 Motion Warping 组件，相机拥有主角。
- 回滚实测：先把 16 个候选文件放入独立副本，执行回滚后 13 个原文件哈希一致、3 个新增文件移除，重新编译成功；原 8 个自动化测试通过，原“不巡逻、箭不掉血”的行为恢复。

完整命令、输出、退出状态见同目录 `VERIFICATION.txt`；文件哈希见 `manifest.json`。`MODIFIED_FILE.zip` 是 16 个改动文件，不包含整套项目。`backup` 保存原文件。`DIFF_FILE.patch` 包含源码及二进制资源差异。

Windows 推荐在关闭本项目编辑器后运行同目录 `ROLLBACK.ps1`；`ROLLBACK.sh` 是 Git Bash 入口，两者共用同目录的 manifest 和 backup。回滚脚本会检查文件是否又被改过，避免覆盖后续编辑。独立副本回滚已经实测，正式项目保留修改版。
