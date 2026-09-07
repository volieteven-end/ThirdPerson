# Countess Boss C++ 实现

工程：`E:/UNREAL/ue projects/ThirdPerson`，UE 5.8.2。此文对应原系统设计的可玩原型；平衡数值保存在 `DA_CountessBoss`，不是最终发行难度。

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

- **绯刃连击**：阶段一两刀、阶段二三刀；16/18/22 伤害，独立段命中集合，段间 0.35 秒。
- **蓄势重斩**：30 伤害，保留慢动作与延迟预警。
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

实测后保留原设计的伤害时间初值、动作播放倍率和额外后摇；修正了 Montage blend-out 被误当成完整动作结束的问题。刀刃查询半径校准为 22 cm；影袭停止距离参数从设计初值 140 调整为 120 cm，以匹配原地 RMB 动画的刀刃触达，最大位移仍为 500 cm，不产生隔空补伤害。

影袭通过 CharacterMovement 的 `FRootMotionSource_MoveToForce` 移动胶囊，先校验导航、胶囊路径和整段地面支撑。使用 `IgnoreZAccumulate`，避免 Recast 表面高度误差被当成起跳。结束只移除自己持有的位移源，不清空其他系统的根运动源。

四向步频按脚部低位后移的接地段估计，而非零根轨迹：前进 **406.06**、后退 **430.21**、左移 **279.87**、右移 **227.27 cm/s**（播放倍率 1）。BlendSpace 依据实际速度调整步频；攻击倍率仍为 1。

## 取消、UI 与复用边界

取消、弹反、破韧、脱战、死亡和 EndPlay 会关闭刀窗，撤销技能位移，停止拖尾，销毁预警并回收该 Boss 的血刃。动作被接受即开始冷却，取消不返还冷却。

Boss 没有第二份独立生命值，所有扣血仍走 `UHealthComponent::ApplyCombatHit`。旧的 `ApplyDamage` / `ApplyDamageFrom` 和投射物 Acquire 接口保留包装；默认箭矢仍在命中后附着并保留 5 秒。

HUD 通过状态事件刷新，败亡后保留 2 秒，再用世界计时器淡出；不依赖 Widget Blueprint 是否启用了 Tick。返回出生点后恢复满血、满韧性、第一阶段并清理 HUD；再次开战仅做 0.75 秒预备。

UE 5.8 的 viewport Position/Size setter 会重置 Anchor，HUD 因而先设置大小和位置、最后设置顶部中心锚点与自身居中对齐。范围预警在转向跟随期间持续按地面法线放置，进入执行期后固定。

## 构建与复测

编辑器资源生成命令可在 UE Python 中重复执行：

```python
import unreal
assert unreal.CountessBossAssetTools.build_countess_assets()
assert unreal.CountessBossAssetTools.build_countess_test_map()
```

编辑器增量构建：

```powershell
& 'D:/Unreal5.8/UE_5.8/Engine/Build/BatchFiles/Build.bat' ThirdPersonEditor Win64 Development '-Project=E:/UNREAL/ue projects/ThirdPerson/ThirdPerson.uproject' -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles
```

自动测试入口为 `ThirdPerson`（包含原有回归）或 `ThirdPerson.Boss`（Boss 专项）。`ThirdPerson.Boss.PIE.SixActionsAt30_60_120` 在真实 PIE、真实蒙太奇/胶囊/刀刃/投射物上测试六招及二阶段变体，并检查世界实际帧步长。

```powershell
& 'D:/Unreal5.8/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/UNREAL/ue projects/ThirdPerson/ThirdPerson.uproject' /Engine/Maps/Entry '-ExecCmds=Automation RunTests ThirdPerson' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:/UNREAL/ue projects/ThirdPerson/Saved/Automation/CountessBossFinal' -unattended -NullRHI -nosound -nop4
```

以导出的 `index.json` 中每项测试状态为准；编辑器进程退出码 0 本身不代表测试通过。原工程原地开发，版本由 Git 保存；不生成工程副本、额外哈希清单或回滚工程。

## 本轮结果

2026-09-07：Editor 与 Game 两个 Win64 Development 目标构建成功；完整 `ThirdPerson` 自动化 **25/25 通过**。最终一轮开启真实 RHI 与离屏渲染，包含三组 PIE 场景；19 个新资产重新加载，蓝图编译和递归依赖检查通过。截图与逐项证据见 `CountessBoss验收记录.md`。

本轮重点修复了保存行为树时黑板引用丢失、Montage Notify 时间被链接到片尾、blend-out 提前截短完整动作、导航表面高度引起影袭离地，以及血条布局和死亡淡出问题。已验证的版本适合作为可玩原型继续调节手感，而非最终平衡结论。
