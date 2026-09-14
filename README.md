# ThirdPerson

基于 Unreal Engine 5 的第三人称动作战斗原型。使用 C++ 与蓝图协作开发，结合现成动画和美术素材，实现剑术连招、格挡弹反、空中战斗、普通敌人 AI 与 Countess Boss 战。

项目重点是把输入、动作、动画通知、命中判定和结束清理衔接起来，并处理低帧率漏判、旧动画回调干扰新动作、多敌人同时进攻等实际问题。

**技术栈：** C++ · UE 5.8 · Enhanced Input · Animation Blueprint / Montage · Behavior Tree / Blackboard · Motion Warping · UMG · Data Asset

## 项目重点

### 1. 数据驱动的动作系统

- 用 `UActionDefinition` 配置招式的动画、消耗、倍率、后续动作、位移与转向策略，通过武器的 `ActionSet` 切换招式集合。
- `UActionComponent` 管理动作许可、输入缓存和动作实例；结合 Montage 与 Anim Notify 控制攻击、连招衔接及取消时机。
- 支持地面连击、疾跑斩、升龙、空中连击、下落攻击、四向闪避、格挡、弹反反击和收拔刀。
- 动画通知与结束回调校验动作代次和 Montage 实例。受击、取消、死亡或播放失败时清理伤害窗口、输入缓存与根运动状态，避免上一招的回调结束下一招。
- 下落攻击将物理触地与动画进度分开：低空提前落地不会截断起手和翻转，落地伤害需要同时满足真实接地和动画命中窗口。

伤害配置也有明确分工：武器提供基础数值，招式提供倍率与命中上下文，战斗组件负责组装和结算，生命组件处理实际受伤结果；Data Asset 和组件不是两套各自扣血的逻辑。

相关代码：[动作定义](Source/ThirdPerson/Actions/ActionDefinition.h) · [动作组件](Source/ThirdPerson/Components/ActionComponent.cpp) · [战斗组件](Source/ThirdPerson/Components/CombatComponent.cpp)

### 2. 沿真实刀刃轨迹的命中检测

仅连接前后两帧的剑根、剑尖位置，可能漏掉高速挥剑中间的弧线。本项目沿刀刃布置多个球形扫掠样本，并在需要时从动画骨骼轨迹补采样中间姿态，再结合角色移动与装备挂接变换进行检测。

- 时间方向最多补 16 个子步，刀刃方向最多 32 个空间样本，限制单帧查询开销。
- 按攻击实例和命中组记录已命中目标，避免同一刀反复扣血，同时允许不同连击段分别结算。
- 实际命中后才触发伤害反馈；默认近战命中顿挫约为 33 毫秒真实时间，多目标命中不叠加持续时间。
- 顿挫逻辑集中在独立组件中，负责恢复时间倍率及暂停、死亡、换图时的清理。

相关代码：[骨骼轨迹采样](Source/ThirdPerson/Animation/SwordBladeSampling.cpp) · [命中反馈](Source/ThirdPerson/Components/HitFeedbackComponent.cpp) · [投射物对象池](Source/ThirdPerson/Weapons/ProjectilePoolSubsystem.cpp)

### 3. 普通敌人协同与 Countess Boss

普通敌人通过行为树、黑板和自定义 Task / Service 完成追击、攻击、后撤及射击；近战通过攻击令牌、站位分配和分离逻辑协调多人围攻，限制同时进攻的数量。

Countess 复用公共生命与伤害链，另用 Boss 动作组件管理遭遇和技能：

- 六类招式：连击、蓄势重斩、范围汲血、突进斩、远程血浪和血宴跃斩。
- 生命阶段切换、独立韧性、破韧、冷却与选招权重；死亡、破韧和转阶段有明确的处理优先级。
- 起手、锁向、地面预警、命中及收招沿动画时间轴组织，短命中窗口支持跨帧检测。
- 攻击间穿插短暂对峙和侧向绕行，保持朝向玩家；移动受导航、视线和场地边界约束。
- 绑定战斗场边界后，玩家仍在场内不会仅因距离出生点过远或短暂遮挡而触发回血重置；遮挡仍影响攻击是否合法。
- 取消和重置时清理刀窗、技能位移、预警与投射物，避免残留伤害。

相关代码：[近战协同](Source/ThirdPerson/AI/MeleeAICombatSubsystem.cpp) · [Boss 动作组件](Source/ThirdPerson/Boss/BossActionComponent.cpp) · [Boss 数据定义](Source/ThirdPerson/Boss/BossDefinition.h)

## 可体验的内容

- **森林遗迹教程：** 分步练习移动、剑术、闪避、防御、血瓶与空中战斗，支持重试和自由练习。
- **双向战斗入口：** 按教程初始出生朝向，右侧进入 Countess 场地，左侧进入随机战斗场，两边都可返回教程。
- **随机战斗场：** 每波生成 3–4 名近战／远程敌人，至少各一名、远程最多两名，清场后等待 3 秒进入下一波。
- **跨地图进度：** 教程训练装备与正式角色进度分开；经验、背包和装备可跨图保留，检查点记录所属地图，避免把旧地图坐标用于新地图。
- **UMG 界面：** 独立开始／继续菜单、背包与属性分页、仅在背包打开时工作的角色模型展示，以及敌人血条和 Boss 状态栏。
- **装备与交互：** 武器配置、道具拾取／使用／丢弃、升级、检查点、宝箱和关卡交互；宝箱可分别配置关闭与开启模型。

## 获取与运行

### 环境

- Windows / Win64；`ThirdPerson.uproject` 当前关联 **UE 5.8**，仓库中的开发验证记录使用 **UE 5.8.2**。
- 可构建该 UE 版本 C++ 工程的 Visual Studio 工具链与 Windows SDK；也可使用 Rider 作为 IDE。
- Git 与 Git LFS。`.uasset`、`.umap`、音频和部分美术资源使用 LFS，不能只获取指针文件。
- 按 `.uproject` 启用对应引擎插件。除了 Motion Warping、Niagara 相关模块，工程还启用了 `MCPClientToolset`、`ModelContextProtocol`、`Terminal` 等开发工具插件；它们未随本仓库附带插件源码。若本机引擎没有这些插件，需要先解决插件可用性，不能假定任意 UE 5.x 安装都能直接打开。

### 拉取工程

```powershell
git lfs install
git clone https://github.com/volieteven-end/ThirdPerson.git
cd ThirdPerson
git lfs pull
```

首次下载包含较多二进制资源。若 LFS 下载失败，应先解决网络、权限或配额问题，不要把资源缺失误判为 C++ 编译问题。`Binaries`、`Intermediate`、`Saved` 与 Derived Data Cache 不作为运行资源提交，需要在本机生成。

### 编译并启动

1. 用匹配的 UE 版本打开 `ThirdPerson.uproject`；需要时生成 IDE 工程文件。
2. 编译 `ThirdPersonEditor / Win64 / Development`。
3. 打开下表中的主菜单或教程地图，点击 Play。首次启动需要等待着色器编译。

也可以在仓库根目录执行以下 PowerShell 命令，将引擎路径替换为本机实际路径：

```powershell
$EngineRoot = 'D:\Unreal5.8\UE_5.8'
$ProjectFile = (Resolve-Path '.\ThirdPerson.uproject').Path
& "$EngineRoot\Engine\Build\BatchFiles\Build.bat" ThirdPersonEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE
```

| 地图 | 内容浏览器路径 | 用途 |
| --- | --- | --- |
| 主菜单 | `/Game/Third/UI/MainMenu/L_MainMenu` | 正式游戏启动入口 |
| 森林遗迹教程 | `/Game/Third/Tutorial/Maps/L_ForestTutorial` | 首次体验、基础操作与双向入口 |
| Countess 场地 | `/Game/Third/Bosses/Countess/Maps/L_CountessBossTest` | Boss 战及技能测试 |
| 随机战斗场 | `/Game/Third/Arenas/Maps/L_RandomArena` | 混合敌人波次挑战 |
| 原始测试关卡 | `/Game/ThirdPerson/Lvl_ThirdPerson` | 综合开发测试 |

正式启动地图与编辑器当前打开的地图可以不同。直接在战斗地图 PIE 不经过主菜单；从菜单选择“开始游戏”会进入教程，有正式存档时会先确认重开。

已有地图和动画资源已经保存，正常游玩不需要运行 `*AssetTools` 或重建地图脚本。部分脚本用于生成／升级资产，执行前应阅读对应说明，避免覆盖自己编辑的内容。

## 默认键鼠操作

| 输入 | 操作 |
| --- | --- |
| WASD / 鼠标移动 | 移动 / 观察 |
| 鼠标左键 | 攻击、接续连招；弹反成功后可反击 |
| 鼠标右键点按 / 长按 | 弹反 / 持续格挡 |
| Shift 短按松开 / 长按并移动 | 闪避 / 冲刺；冲刺时左键为疾跑斩 |
| 空格 | 跳跃；解锁后可二段跳 |
| C + 左键 | 升龙，随后左键可接空中连击 |
| 空中 R | 下落攻击 |
| 鼠标中键 | 锁定 / 解除锁定 |
| 锁定时 Alt + 鼠标横移 | 切换锁定目标 |
| B / X | 剑术 Buff / 收拔刀 |
| Q / E | 使用消耗品 / 交互 |
| Tab / Esc | 背包与属性 / 暂停菜单 |

空格用于跳跃，Shift 用于冲刺与短按闪避。按键以角色当前使用的 Enhanced Input 映射为准，默认映射位于 `Content/Third/Input/IMC_Default`；更换或修改角色蓝图后可能不同。

## 代码导航

```text
Source/ThirdPerson/
├── Actions/       招式定义、动作集及编辑器配置工具
├── Components/    动作、战斗、生命、装备、背包与命中反馈
├── Animation/     动画实例、通知、刀刃轨迹和武器挂接
├── Character/     主角输入、移动、锁定与控制器
├── AI/            普通敌人、行为树节点和近战协同
├── Boss/          Countess 遭遇、技能、韧性、动画与预警
├── Arena/         场地边界、地图传送与随机波次
├── Tutorial/      教学课程、目标判定与训练状态
├── Save/          检查点、正式角色进度与存档槽
├── UI/            菜单、背包、属性、角色展示与状态栏
├── Weapons/       武器、投射物、对象池与特效接口
├── World/         门、宝箱和关卡出口
└── Tests/         UE Automation 与实际 PIE 回归
```

项目蓝图、动作与地图主要位于 `Content/Third/`。源码中的编辑器资产工具由 Editor 构建条件隔离，不应在游戏运行时调用。

## 测试与实现记录

`Source/ThirdPerson/Tests/` 包含动作许可与缓存、刀刃判定、格挡弹反、Boss 状态与取消、投射物复用、教程、跨图存档、UI 和敌人表现等测试。部分测试使用真实地图、输入与动画运行 PIE，而不只检查配置值。

可在 UE 的自动化测试面板中筛选以下前缀，按需要分组运行：

| 筛选前缀 | 检查内容 |
| --- | --- |
| `ThirdPerson.ActionSystem` / `ThirdPerson.Sword` | 动作、连招、取消、移动与武器资源契约 |
| `ThirdPerson.Boss` | Boss 结算、破韧、阶段、命中窗口与重置 |
| `ThirdPerson.Projectiles` | 单次伤害、碰撞和对象池复用 |
| `ThirdPerson.Tutorial` / `ThirdPerson.Arena` | 课程、往返、奖励和波次 |
| `ThirdPerson.Combat.PIE.DiveFullPlayback` | 不同高度及 30 / 60 / 120 FPS 模拟步长下的完整下砸 |
| `ThirdPerson.CombatUI` / `ThirdPerson.EnemyPresentation` | 菜单、存档策略、分页、模型展示、血条与死亡动作 |

涉及真实关卡的测试应使用独立测试存档，可通过启动参数 `-TPCSaveSlot=<唯一测试槽名>` 与正式 `PlayerSave` 隔离。带渲染的 PIE、UMG 和视觉测试需要正常的渲染环境，不应一律使用 NullRHI。

已有验证记录与配置入口：

- [主角动作系统实现说明](DesignDocs/SwordActionSystem/主角动作系统实现说明.md)
- [Countess Boss 实现说明](DesignDocs/CountessBoss/CountessBoss实现说明.md)
- [教程、双向入口与随机战斗场](Docs/TutorialArenaExpansion.md)
- [命中顿挫、Boss 对峙与 UMG](Docs/CombatAndUIUpgrade.md)
- [武器换手与动画特效](Docs/SwordMotionAndAuthoredFX.md)
- [敌人血条、死亡动画与相机碰撞](Docs/EnemyPresentationAndCamera.md)

这些文件记录的是对应阶段的实现与验证，部分包含后续改动前的数值和已知问题。更改连段或动画通知后，资源契约测试也需要同步检查；历史测试通过不代表当前所有配置、平台和打包版本均已验证。30 / 60 / 120 FPS 固定步长回归用于检查逻辑一致性，不等同于 GPU 性能基准。

## 项目边界与素材说明

- 这是持续迭代中的单机动作原型，不是完整商业游戏，也没有把当前战斗系统实现为 GAS 或多人网络战斗。
- 动画、角色、场景、特效和音效包含现成第三方素材，例如 Paragon Countess、SlashTrailElemental 等；项目工作重点是玩法实现、资源接入、动画与判定衔接及问题修复，不将这些素材描述为原创。
- 第三方素材的授权不因放入本仓库而改变。使用或分发前请核对各自来源与授权；本仓库没有为第三方内容重新授予许可证。
- 打包兼容性、不同硬件上的性能和最终战斗平衡需要针对目标环境继续验证，不以功能演示或单项自动化结果作保证。
