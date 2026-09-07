# Countess Boss 验收记录

日期：2026-09-07，UE 5.8.2（CL 56702186），Win64 Development。

## 结论与实测条件

- Editor 和 Game 两个目标均返回 `Result: Succeeded`，进程退出码 **0**。
- 完整 `ThirdPerson` 自动化 **25/25 通过**：16 Success、9 SucceededWithWarnings、0 failed、0 notRun，进程退出码 **0**。没有以进程码代替测试 JSON。
- 最终轮使用真实 RHI、1280×720 离屏渲染，而非 NullRHI。三组真实 PIE 分别覆盖遭遇/移动/死亡、导航/视线/重置/再入场，以及六招和二阶段变体的 30/60/120 FPS 执行。
- 19 个新增 UE 资产重新从磁盘加载；Boss BP、AnimBP 和 Widget BP 重新编译无错误。递归检查 158 个 /Game 包，`CountessPlayerCharacter` 依赖为 **0**；保存的 BT 正确指向 BB_CountessBoss。
- 原 Paragon 文件没有改写；新增资产集中在 `/Game/Third/Bosses/Countess`。本轮不包含 Cook/发行包。

六招测试将目标固定，并使用 1000 测试生命值，使实际刀刃、投射物、动画和位移结果可复现；遭遇测试另行运行真实行为树与导航。该结果证明实现和这些场景的回归通过，不作为最终战斗平衡评价。

## 关键规则证据

| 规则 | 实际验证 |
|---|---|
| 六招/阶段 | 六招均从保存 BP 执行；第二阶段三连斩和影袭追加刀分别为 56/42 总伤害；阶段一对应 34/24 |
| 命中 | 真实动画驱动左右刀刃扫掠；同一段多帧/双刀只扣一次，下一段独立；Siphon 按实际扣血回血 |
| 防御 | 物理弹反、中断并扣 40 韧性；血术按普通格挡扣精力；血宴越过格挡，闪避无敌仍优先；背后攻击不误格挡 |
| 削韧/阶段边界 | 玩家普通/空中/升龙/下砸总削韧为 10/12/30/25；升龙不重复/不击飞；跨阈值同时破韧先眩晕再转阶段；致死优先，回血不降阶段 |
| 清理 | 旧 Montage 实例/动作序号回调失效；取消回收自己的血刃、预警和位移源，保留外部位移源 |
| 影袭 | 平地真实 CMC 位移约 367–377 cm；Mesh 相对胶囊不漂移，玩家控制旋转保持；路径几何检查覆盖墙、低台阶和悬崖不可达点 |
| AI | 实际接近开战、面向玩家后退；上下文服务持续更新；1.5 秒遮挡保留目标，5 秒遮挡后脱战；玩家死亡后实际导航回家 |
| 再战 | HUD 移除、目标清空、恢复 1500 生命/100 韧性/第一阶段，第二次只进行 0.75 秒预备 |
| HUD/预警 | 顶部中心 Anchor 与 Alignment 断言通过，真实渲染可见；斜坡预警跟随更新后仍与地面法线一致 |
| 死亡/复用 | 同帧取消，最高优先级 Death 保持末帧，约 5 秒销毁；300 经验与击杀只一次，不调用全局 WinGame；已有 13 项主角/箭矢测试全部通过 |
| 旧箭矢 | 伤害只一次、友伤过滤、命中后关闭碰撞并附着、5 秒回收计时、池复用清理旧状态均通过 |

## 实测截图

血宴预警期间，顶部生命为测试值 1000/1500，韧性满值，阶段 II。截图直接来自最终 PIE，没有后期合成。

![Countess Boss PIE](E:/UNREAL/ue%20projects/ThirdPerson/DesignDocs/CountessBoss/CountessBoss_PIE.png)

## 复测命令

以下路径对应本机工程，改机时替换 UE 和工程根路径。

```powershell
& 'D:/Unreal5.8/UE_5.8/Engine/Build/BatchFiles/Build.bat' ThirdPersonEditor Win64 Development '-Project=E:/UNREAL/ue projects/ThirdPerson/ThirdPerson.uproject' -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles
& 'D:/Unreal5.8/UE_5.8/Engine/Build/BatchFiles/Build.bat' ThirdPerson Win64 Development '-Project=E:/UNREAL/ue projects/ThirdPerson/ThirdPerson.uproject' -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles
& 'D:/Unreal5.8/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/UNREAL/ue projects/ThirdPerson/ThirdPerson.uproject' /Engine/Maps/Entry '-ExecCmds=DisableAllScreenMessages,Automation RunTests ThirdPerson' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=E:/UNREAL/ue projects/ThirdPerson/Saved/Automation/CountessBossFinal' -unattended -RenderOffscreen -ResX=1280 -ResY=720 -BossVisualAudit -nosound -nop4 -stdout '-abslog=E:/UNREAL/ue projects/ThirdPerson/Saved/Logs/CountessFinalRenderTests.txt'
& 'D:/Unreal5.8/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/UNREAL/ue projects/ThirdPerson/ThirdPerson.uproject' -run=pythonscript '-script=E:/UNREAL/ue projects/ThirdPerson/DesignDocs/CountessBoss/verify_assets.py' -NullRHI -unattended -nosound -nop4
```

完整日志位于 `E:/UNREAL/ue projects/ThirdPerson/Saved/Logs/CountessEditorBuild.txt`、`CountessRuntimeBuild.txt`、`CountessFinalRenderTests.txt` 和 `CountessDependencyAudit.txt`；结果 JSON 位于 `E:/UNREAL/ue projects/ThirdPerson/Saved/Automation/CountessBossFinal/index.json`，资产审计位于 `E:/UNREAL/ue projects/ThirdPerson/Saved/CountessBossDesignAudit/final_asset_audit.json`。

日志保留了模板战斗日志、临时 World 销毁/Crowd 导航提示和引擎 MotionVector 提示，因此九项记为 SucceededWithWarnings；这些条目没有 Error 事件或失败断言。

## 各测试结果

| 测试 | 结果 |
|---|---|
| `ThirdPerson.ActionSystem.AirAndMovement` | 通过（含日志提示） |
| `ThirdPerson.ActionSystem.CombatLifecycle` | 通过 |
| `ThirdPerson.ActionSystem.ComboBuffer` | 通过 |
| `ThirdPerson.ActionSystem.DashCancelGuards` | 通过（含日志提示） |
| `ThirdPerson.ActionSystem.DashCancelLifecycle` | 通过 |
| `ThirdPerson.ActionSystem.DashComboWindow` | 通过 |
| `ThirdPerson.ActionSystem.DodgeRootMotionHandoff` | 通过 |
| `ThirdPerson.ActionSystem.MovementRules` | 通过 |
| `ThirdPerson.ActionSystem.RootTurnAndDeath` | 通过（含日志提示） |
| `ThirdPerson.ActionSystem.SprintActionInterlock` | 通过 |
| `ThirdPerson.Boss.PIE.EncounterMovementDeath` | 通过（含日志提示） |
| `ThirdPerson.Boss.PIE.NavigationLOSResetReentry` | 通过（含日志提示） |
| `ThirdPerson.Boss.PIE.SixActionsAt30_60_120` | 通过（含日志提示） |
| `ThirdPerson.Boss.AssetContract` | 通过 |
| `ThirdPerson.Boss.CancellationAndPool` | 通过 |
| `ThirdPerson.Boss.CrossedWindowsAt30_60_120` | 通过 |
| `ThirdPerson.Boss.DefenseOutcomes` | 通过 |
| `ThirdPerson.Boss.HitDedupAndActualLifesteal` | 通过 |
| `ThirdPerson.Boss.NativeAnimationAndSavedBlueprint` | 通过 |
| `ThirdPerson.Boss.PlayerHitContextsAndBackBlock` | 通过 |
| `ThirdPerson.Boss.PoisePhaseOrdering` | 通过 |
| `ThirdPerson.Boss.SelectionAndReset` | 通过 |
| `ThirdPerson.Projectiles.CollisionInvariant` | 通过（含日志提示） |
| `ThirdPerson.Projectiles.DamageOnceAndFriendlyFire` | 通过（含日志提示） |
| `ThirdPerson.Projectiles.ReuseAndReset` | 通过（含日志提示） |

## 六招逐次结果

输入距离依次为 Combo 110、DelayedSlash 150、Siphon 200、ShadowRush 500、BloodWave 500、BloodFeast 200 cm；以下 Duration 包含预警、完整动画、段间停顿和后摇。Action 6/7 为二阶段连击/影袭。

```text
Action=0 Phase=1 FPS=30 Damage=34 Duration=3.133 Travel=0.00
Action=1 Phase=1 FPS=30 Damage=30 Duration=2.633 Travel=0.00
Action=2 Phase=1 FPS=30 Damage=22 Duration=2.300 Travel=0.00
Action=3 Phase=1 FPS=30 Damage=24 Duration=2.500 Travel=377.31
Action=4 Phase=1 FPS=30 Damage=20 Duration=2.300 Travel=0.00
Action=5 Phase=2 FPS=30 Damage=38 Duration=5.067 Travel=0.00
Action=6 Phase=2 FPS=30 Damage=56 Duration=4.400 Travel=0.00
Action=7 Phase=2 FPS=30 Damage=42 Duration=3.767 Travel=377.31
Action=0 Phase=1 FPS=60 Damage=34 Duration=3.100 Travel=0.00
Action=1 Phase=1 FPS=60 Damage=30 Duration=2.600 Travel=0.00
Action=2 Phase=1 FPS=60 Damage=22 Duration=2.267 Travel=0.00
Action=3 Phase=1 FPS=60 Damage=24 Duration=2.483 Travel=372.83
Action=4 Phase=1 FPS=60 Damage=20 Duration=2.267 Travel=0.00
Action=5 Phase=2 FPS=60 Damage=38 Duration=5.067 Travel=0.00
Action=6 Phase=2 FPS=60 Damage=56 Duration=4.350 Travel=0.00
Action=7 Phase=2 FPS=60 Damage=42 Duration=3.733 Travel=372.83
Action=0 Phase=1 FPS=120 Damage=34 Duration=3.100 Travel=0.00
Action=1 Phase=1 FPS=120 Damage=30 Duration=2.600 Travel=0.00
Action=2 Phase=1 FPS=120 Damage=22 Duration=2.267 Travel=0.00
Action=3 Phase=1 FPS=120 Damage=24 Duration=2.483 Travel=367.01
Action=4 Phase=1 FPS=120 Damage=20 Duration=2.267 Travel=0.00
Action=5 Phase=2 FPS=120 Damage=38 Duration=5.067 Travel=0.00
Action=6 Phase=2 FPS=120 Damage=56 Duration=4.350 Travel=0.00
Action=7 Phase=2 FPS=120 Damage=42 Duration=3.733 Travel=367.01
```

## 新增资产

- `/Game/Third/Bosses/Countess/AI/BB_CountessBoss` — /Script/AIModule.BlackboardData
- `/Game/Third/Bosses/Countess/AI/BT_CountessBoss` — /Script/AIModule.BehaviorTree
- `/Game/Third/Bosses/Countess/Animations/ABP_CountessBoss` — /Script/Engine.AnimBlueprint
- `/Game/Third/Bosses/Countess/Animations/AM_Countess_BloodFeast_1` — /Script/Engine.AnimMontage
- `/Game/Third/Bosses/Countess/Animations/AM_Countess_BloodWave_1` — /Script/Engine.AnimMontage
- `/Game/Third/Bosses/Countess/Animations/AM_Countess_Combo_1` — /Script/Engine.AnimMontage
- `/Game/Third/Bosses/Countess/Animations/AM_Countess_Combo_2` — /Script/Engine.AnimMontage
- `/Game/Third/Bosses/Countess/Animations/AM_Countess_Combo_3` — /Script/Engine.AnimMontage
- `/Game/Third/Bosses/Countess/Animations/AM_Countess_DelayedSlash_1` — /Script/Engine.AnimMontage
- `/Game/Third/Bosses/Countess/Animations/AM_Countess_ShadowRush_1` — /Script/Engine.AnimMontage
- `/Game/Third/Bosses/Countess/Animations/AM_Countess_ShadowRush_2` — /Script/Engine.AnimMontage
- `/Game/Third/Bosses/Countess/Animations/AM_Countess_Siphon_1` — /Script/Engine.AnimMontage
- `/Game/Third/Bosses/Countess/Animations/BS_CountessLocomotion` — /Script/Engine.BlendSpace
- `/Game/Third/Bosses/Countess/BP_CountessBoss` — /Script/Engine.Blueprint
- `/Game/Third/Bosses/Countess/DA_CountessBoss` — /Script/ThirdPerson.BossDefinition
- `/Game/Third/Bosses/Countess/Maps/L_CountessBossTest` — /Script/Engine.World
- `/Game/Third/Bosses/Countess/Materials/M_BossTelegraph` — /Script/Engine.Material
- `/Game/Third/Bosses/Countess/Meshes/SK_CountessBoss` — /Script/Engine.SkeletalMesh
- `/Game/Third/Bosses/Countess/UI/WBP_BossStatus` — /Script/UMGEditor.WidgetBlueprint
