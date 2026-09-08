# Countess Boss C++ 实现

工程：`E:/UNREAL/ue projects/ThirdPerson`，UE 5.8.2。2026-09-08 已升级为可读起手版本（`AnimationRevision=2`）；平衡数值保存在 `DA_CountessBoss`，不是最终发行难度。

## 直接使用

1. 打开 `/Game/Third/Bosses/Countess/Maps/L_CountessBossTest`，点击 Play。Boss 直接放置在关卡中，靠近且可见时开战。
2. 在自己的关卡放置 `/Game/Third/Bosses/Countess/BP_CountessBoss`，并覆盖可行走区域的 NavMeshBoundsVolume。不要通过普通敌人的循环刷怪器生成它。
3. 数据入口为 `/Game/Third/Bosses/Countess/DA_CountessBoss`。生命 1500，半血进入第二阶段；阶段一/二韧性 100/120；击杀经验 300。
4. 控制台 `tp.Boss.Debug 1` 显示状态、动作序号、韧性、位移源 ID、刀刃端点和扫描半径。`tp.Boss.Debug 0` 关闭。日志中 `LogTemp Verbose` 可查看状态转换与实际命中结果。

## 代码分工

源码目录：`E:/UNREAL/ue projects/ThirdPerson/Source/ThirdPerson/Boss`。

| 入口 | 职责 |
|---|---|
| `ACountessBossCharacter` | 继承 EnemyCharacter；隐藏小怪血条，覆盖受击、升龙、弹反、死亡；复用父类唯一的奖励结算 |
| `UBossActionComponent` | 遭遇、六招、单动作所有权、冷却、阶段/韧性、预警、刀刃查询、位移、取消与返回出生点 |
| `UBossDefinition` | 起始数值、各阶段权重、动画、特效、UI 和行为树配置 |
| `ACountessBossAIController` / BossBehaviorNodes | 八个优先状态分支、0.1 秒上下文服务、导航追击、后退和绕行 |
| `UCountessBossAnimInstance` | 提供真实局部速度、受击、眩晕、阶段和死亡参数；独立 AnimBP 使用这些数据 |
| `UBossDamageWindowNotifyState` | 校验动作对应的 Montage 实例，标记伤害窗口；不会接旧玩家连招 Notify |
| `ABossBloodWave` | 复用现有按类分类的投射物池；速度 900、射程 1200，命中立即回收 |
| `ABossTelegraph` / `UBossStatusWidget` | 贴地形状预警、独立顶部生命/韧性/阶段 HUD |
| `UCountessBossAssetTools` | 编辑器资源生成与测试关卡导航构建；运行时不依赖编辑器模块 |

Actor 提供 `StartEncounter(Player)`、`RequestEncounterReset()`、`GetBossPhase()`、`GetPoisePercent()`；通常由控制器自动调用。蓝图只负责配置，不需要在 Event Graph 再写一套技能执行逻辑。

## 六招与规则

- **绯刃连击**：阶段一两刀、阶段二三刀；16/18/22 伤害，独立段命中集合。第一刀身体准备不少于 0.55 秒，后续每刀不少于 0.35 秒，不回待机等待。
- **蓄势重斩**：30 伤害，慢速攻击素材的准备段不少于 0.85 秒，击出段不降速。
- **环刃汲血**：半径 260、垂直中心差最多 140；有视线才命中。回复实际扣血的 50%，上限 20。
- **影袭斩**：阶段一一次 24 伤害，阶段二可追加 18；追加前再次预警，目标仍需在 240 内。
- **暗潮血刃**：20 伤害，非追踪；和普通箭矢共用池，但不保留插身/插地效果。
- **血宴跃斩**：阶段二开放，38 伤害；预警半径 330、命中半径 300。预警保持到落地判定，不抓取、搬运玩家或更换相机。

物理刀招可格挡/弹反；环刃和血刃在弹反窗口内仍按普通格挡扣精力；血宴不受格挡、仍受闪避无敌限制。普通/空中/升龙/下砸分别填入总削韧 10/12/30/25；升龙覆盖函数不再次削韧，也不击飞 Boss。

同一命中跨半血并破韧时，先兑现 2.4 秒破韧，再转阶段；死亡优先。恢复期 0.8 秒只免削韧、不免生命伤害。阶段转换 1.5 秒免伤；吸血不会退回阶段一。

## 动画、命中与校准

已保存独立的 `ABP_CountessBoss`、四向 `BS_CountessLocomotion`、九个全身 Montage、`BT_CountessBoss`、`BB_CountessBoss`、`WBP_BossStatus`。动画图使用 DefaultSlot、Local Space Apply Additive、Inertialization 和最高优先级 Death Evaluator，死亡末帧不循环。

只创建一个带四个 Mesh Socket 的 Countess 网格变体；原 Paragon 骨架、Sequence 和特效保持原样。命中沿左右刀刃的多点轨迹做 Sphere Sweep，同一段共用去重集合。选招距离不是必命中范围：例如近身目标可吃到完整连击，离刀刃较远时第二刀允许真实打空，不用范围伤害补中。

**伤害时间的权威来源是已配置 Montage 的 Boss NotifyState。** 数据中的 HitStart/HitEnd 仅用于新资源生成/无 Montage 原生配置，不会覆盖已配置 Montage。执行器检测播放位置是否跨过该 Notify 时间区间，短窗口在低帧率也可触发；旧 Montage 实例和旧 ActionSerial 的回调会失效。

新版在 Boss 专用 Montage 中分段延长准备，运行 `Montage_Play` 的倍率始终为 1，命中的源动画片段也保持倍率 1。吸血与血波接入对应 `target_transition`，连段结束使用独立恢复 Montage。下表均从该刀 Montage 开始计时；可读准备下限不包含 0.08 秒淡入，二阶段不缩短。伤害、刀光、特效与突进均沿调整后的同一时间轴。

| 招式 | 首次事件（秒） | 锁向（秒） | 说明 |
|---|---:|---:|---|
| 连击首刀 | 0.630 | 0.480 | 0.55 秒净准备 |
| 连击后续刀／突进追加刀 | 0.430 | 0.280 | 0.35 秒净准备 |
| 重斩 | 0.930 | 0.730 | 0.85 秒净准备 |
| 吸血 | 0.830 | 0.680 | 0.75 秒净准备 |
| 突进 | 0.730 移动，1.040 刀窗 | 0.580 | 移动在 1.030 秒结束；到达后才判刀 |
| 血波 | 1.000 | 0.800 | 准备连接原释放动作 |
| 血宴 | 2.130 | 0.500 | 2.05 秒净身体准备，无额外站桩等待 |

这些数值描述保存资产，伤害执行仍读取 Notify，而不是新增计时器。取消、受击、死亡优先级不变；持盾只给 Boss 小幅肩部反作用，成功弹刀取消剩余连段并保留 0.7 秒窗口，破韧仍为 2.4 秒。动画收招时新动作请求被拒绝，后摇不少于原配置（大部分为 0.55 秒；实际恢复片段也必须播完）。

刀刃查询半径仍为 22 cm；影袭停止距离仍为 120 cm，最大位移仍为 500 cm，不产生隔空补伤害。

影袭通过 CharacterMovement 的 `FRootMotionSource_MoveToForce` 移动胶囊，先校验导航、胶囊路径和整段地面支撑。使用 `IgnoreZAccumulate`，避免 Recast 表面高度误差被当成起跳。结束只移除自己持有的位移源，不清空其他系统的根运动源。

独立 AnimBP 接入四向起步、完整步态循环、停步、反向 Pivot、90°／180°原地转身，以及左右 Circle 混合空间。循环以左右脚落地标记同步；起停使用重新初始化的非循环播放器接入同一同步组，防止长转身切短起步时继承越界的同步时间。导航 0.4 秒刷新目标不会重播起步。移动／停止阈值 35／12 cm/s，方向有锁存，普通混合 0.15 秒；全身战斗动作可以打断这些过渡。

新版四向整周期的接地步速为前进 **376.65**、后退 **414.79**、左移 **275.65**、右移 **214.84 cm/s**（倍率 1）。左右 Circle 分别测量自己的四向步速，按混合权重与真实速度共同校准步频，不能直接套用直线侧移速度。追赶由 CMC 路径转向，侧移/原地转向由 Boss 动作组件负责，Controller 不再同时 SetFocus 扭转角色。

## 取消、UI 与复用边界

取消、弹反、破韧、脱战、死亡和 EndPlay 会关闭刀窗，撤销技能位移，停止拖尾，销毁预警并回收该 Boss 的血刃。动作被接受即开始冷却，取消不返还冷却。

Boss 没有第二份独立生命值，所有扣血仍走 `UHealthComponent::ApplyCombatHit`。旧的 `ApplyDamage` / `ApplyDamageFrom` 和投射物 Acquire 接口保留包装；默认箭矢仍在命中后附着并保留 5 秒。

HUD 通过状态事件刷新，败亡后保留 2 秒，再用世界计时器淡出；不依赖 Widget Blueprint 是否启用了 Tick。返回出生点后恢复满血、满韧性、第一阶段并清理 HUD；再次开战仅做 0.75 秒预备。

UE 5.8 的 viewport Position/Size setter 会重置 Anchor，HUD 因而先设置大小和位置、最后设置顶部中心锚点与自身居中对齐。范围预警在转向跟随期间持续按地面法线放置，进入执行期后固定。

## 构建与复测

已存在 Boss 的动画定向升级命令（仅保存 Boss 动画与配置；不覆盖地图、角色蓝图或 Paragon 原资产）：

```python
import unreal
assert unreal.CountessBossAssetTools.upgrade_readable_animations()
```

下列命令是从零创建原型资产／测试地图的旧入口；已有自定义地图时不要用它重建地图：

```python
import unreal
assert unreal.CountessBossAssetTools.build_countess_assets()
assert unreal.CountessBossAssetTools.build_countess_test_map()
```

编辑器增量构建：

```powershell
& 'D:/Unreal5.8/UE_5.8/Engine/Build/BatchFiles/Build.bat' ThirdPersonEditor Win64 Development '-Project=E:/UNREAL/ue projects/ThirdPerson/ThirdPerson.uproject' -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles
```

自动测试入口为 `ThirdPerson`（包含原有回归）或 `ThirdPerson.Boss`（Boss 专项）。`ThirdPerson.Boss.PIE.SixActionsAt30_60_120` 在真实 PIE、真实蒙太奇/胶囊/刀刃/投射物上测试六招及二阶段变体，并检查世界实际帧步长。新增 `ThirdPerson.Boss.Readability.AssetTimelines`、`ThirdPerson.Boss.Readability.PIE.LocomotionAt30_60_120` 与 `ThirdPerson.Boss.Readability.PIE.PlayerDefenseAt30_60_120`，覆盖净准备、锁向、收招、连续绕行、急停反转和实际玩家攻防。

添加 `-BossReadableVideo` 并使用真实 RHI 可导出固定 30 Hz 模拟、隔帧采集的 15 FPS 连续画面；按 15 FPS 编码即为 1 倍速。攻击采集隐藏地面提示，时间线写入 `Saved/BossReadability`。测试使用临时武器伤害副本，不改玩家已经调过的 `DA_TestSword`。

```powershell
& 'D:/Unreal5.8/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/UNREAL/ue projects/ThirdPerson/ThirdPerson.uproject' /Engine/Maps/Entry '-ExecCmds=Automation RunTests ThirdPerson' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:/UNREAL/ue projects/ThirdPerson/Saved/Automation/CountessBossFinal' -unattended -NullRHI -nosound -nop4
```

以导出的 `index.json` 中每项测试状态为准；编辑器进程退出码 0 本身不代表测试通过。原工程原地开发，版本由 Git 保存；不生成工程副本、额外哈希清单或回滚工程。

## 本轮结果

2026-09-08 可读性改造：Editor 与 Game 的 Win64 Development 构建成功；最终 Boss 专项 **15/15 通过**（10 项无警告、5 项含已有场景/引擎日志警告、0 失败），相关玩家战斗回归 **21/21 通过**。Boss 专用动画、混合空间、AnimBP 和配置已实际保存，并在新编辑器进程中重新加载测试。攻击时序覆盖两阶段所有合法招式的 33 个 FPS/招式组合，另有 30 个移动场景及 30 个玩家攻防场景。

正常速度的主观动画验收仍待人工播放确认：已检查连续采样画面和逐帧运行数据，但当前可用查看工具不能完成正常速度视频观察，不能据此宣称最终自然度验收通过。已输出 `Saved/BossReadability/attacks_1x.mp4` 与 `locomotion_1x.mp4`（固定 30 Hz 模拟、15 FPS 采集、1 倍速编码，攻击画面隐藏地面提示）。当前没有因已证实的时序失败而停用招式；若后续正常播放发现某招无法兼顾自然度与准备下限，应降低其权重至 0 后再交付该验收项。详细证据与此限制见 `CountessBoss验收记录.md` 的 2026-09-08 节。

2026-09-07：Editor 与 Game 两个 Win64 Development 目标构建成功；完整 `ThirdPerson` 自动化 **25/25 通过**。最终一轮开启真实 RHI 与离屏渲染，包含三组 PIE 场景；19 个新资产重新加载，蓝图编译和递归依赖检查通过。截图与逐项证据见 `CountessBoss验收记录.md`。

本轮重点修复了保存行为树时黑板引用丢失、Montage Notify 时间被链接到片尾、blend-out 提前截短完整动作、导航表面高度引起影袭离地，以及血条布局和死亡淡出问题。已验证的版本适合作为可玩原型继续调节手感，而非最终平衡结论。
