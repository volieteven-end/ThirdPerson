# 教程入口、战斗场及完整下落攻击修改记录

## 已确认的规则

- 教程出生朝向右侧去 Countess，左侧去独立随机战斗场；入口按 E，双向返回。
- 教程训练状态与正式装备、经验和背包隔离；本次会话保留课程进度。
- 每波 3–4 人，至少一名近战和远程，最多两名远程，清完等待 3 秒。
- 退场或重试重置挑战；战败重试在当前场地入口满状态重生，保留正式奖励。
- 下落攻击物理触地不截断起手和翻转，砸地通知到达后才结算落地伤害。

## 修改流程（2026-09-13 已实施）

1. 核对源工程与现有修改。用户已确认 UE 保存并关闭；不修改原有连招循环、闪避教学和手工场景编辑。
2. 提取 `TPCPlayerProgress` 供检查点和跨图逻辑共用；存档增加检查点归属、装备及二段跳状态，旧档保留兼容读取。
3. 新增 `ArenaTravelSubsystem`，分开传递教程会话与正式进度；到达位置采用命名 PlayerStart，不套用另一地图检查点。
4. 新增 `ArenaPortal`、`ArenaBounds`、`ArenaWaveDirector` 与 `ArenaGameMode`；Boss 绑定明确场地边界后不再因场内距离/遮挡超时重置。
5. 新增局部地图增补工具。生成物标记 `ArenaExpansion_v1`，已有地图不删除或整体重建；随机地图仅在不存在时新建。
6. 调整下落攻击运行时段落衔接：触地开放接地屏障，继续未播放的 Start/Descent；仅可跳过空中维持 Loop，不重建蒙太奇。
7. 编译 Editor 后运行局部地图增补脚本，保存教程、Boss 和新随机场。复用 green_Island 的拱门、柱子、铺地、断墙和树，以及 RoyalCapital 石材；未改动素材包原资源。
8. 补充往返、存档隔离、混合波次、Boss 边界和下砸自动化。测试中修复 PIE 地图前缀造成的目的地匹配问题，以及返回教程后蓝图默认装备覆盖训练装备的问题；训练配置在下一帧重新应用，不重置课程。
9. 实际渲染三张地图并检查截图：按资源真实尺寸将拱门缩至约 4.6 米宽、3.8 米高，调整新随机场曝光；仅将新增教程入口向前移 330 厘米，避开原有练习牌。原教程牌、Boss 测试障碍和宝箱保留。
10. 完成 Editor／Game 编译和最终 31 项回归。只暂存本轮源代码片段及相关地图；原有第四段连招循环、闪避教学和无关素材修改未混入源码提交。

## 地图与进入方式

打开 `/Game/Third/Tutorial/Maps/L_ForestTutorial`，从初始出生点面向前方，右侧石门去 Countess，左侧石门去随机战斗场。靠近并看向门内交互区域，按 **E**；到达另一侧也用 E 返回。

| 地图 | 入口或抵达点（UE 厘米坐标） | 返回教程位置 |
| --- | --- | --- |
| 教程 `L_ForestTutorial` | Boss 门 `(-4700,-1750,140)`；随机门 `(-4700,-3050,140)` | `FromBoss=(-4700,-2030,112)`；`FromRandomArena=(-4700,-2770,112)` |
| Boss `L_CountessBossTest` | `ArenaArrival=(-2925,0,112)`；返回门 `(-3265,0,140)` | `FromBoss` |
| 随机 `L_RandomArena` | `ArenaArrival=(-2425,0,112)`；返回门 `(-2765,0,140)` | `FromRandomArena` |

Boss 保留原约 50×50 米地面，随机场约 40×40 米。抵达区位于西侧，走入场地后才开始挑战；随机场有外围墙、入口缺口和少量低掩体，不走原击杀数量通关逻辑。返回或死亡重试会重新加载世界，敌人、对象池投射物及本轮计时器随世界清理，不通过伪造死亡发放奖励。

## 调整入口

- `AArenaPortal`：Destination、ArrivalTag、Label。
- `AArenaBounds.CombatBoundary`：场地盒大小与位置；覆盖整个可玩地面及跳跃高度。
- `UBossActionComponent`：ArenaBoundary、ArenaExitGrace（2 秒）；没有绑定时维持原脱战机制。
- `AArenaWaveDirector`：敌人蓝图、WaveDelay（3 秒）、PlayerSpawnClearance（700 厘米）、RandomSeed（0 为每局随机）。
- 地图：`L_ForestTutorial`、`L_CountessBossTest`、`/Game/Third/Arenas/Maps/L_RandomArena`。
- 返回标记：教程 `FromBoss` / `FromRandomArena`；战斗场 `ArenaArrival`。
- Boss 场地盒：中心 `(0,0,450)`，半尺寸 `(2470,2470,1100)`；随机场盒：中心 `(0,0,450)`，半尺寸 `(1950,1950,1100)`。
- 边界判断使用角色脚底加 20 厘米，额外容差 25 厘米。Boss 寻路目标限制在场内，突进目的地也检查边界；场内遮挡不重置，但仍参与攻击合法性检查。
- 两张战斗场地面为 Z=0；角色跌至 Z<-800 时进入正常死亡／重试流程。若后续整体移动地图高度，要同时调整此保护阈值。

## 关键代码与状态规则

- `Source/ThirdPerson/Arena/ArenaTravelSubsystem.*`：一次性抵达标记、防连续传送、目的地图存在性校验、保存成功后才离场；运行时传送失败会释放锁并显示错误。
- `Source/ThirdPerson/Save/TPCPlayerProgress.*`：通用等级、经验、已选升级、待选升级、血量、耐力、背包、装备、拔刀和二段跳状态。装备只保存资产引用，不跨世界保留武器 Actor。
- `TPCSaveGame`：新增 `CheckpointMap`、`bHasCheckpoint`、装备及能力字段；正式进度合并保存不会覆盖原地图的检查点坐标、门状态和击杀进度。无归属信息的旧检查点只用于原 `Lvl_ThirdPerson`；旧档缺失装备字段时保持角色默认装备。
- 教程只在 GameInstance 会话中保存课程、完成状态、当前目标和计数；不保存旧敌人指针、事件编号或动作。返回时使用训练装备，重新启动整个游戏则不保留本次课程快照。
- 无正式存档时，从战斗场新生成的正式角色默认配置建立进度，绝不从教程训练背包初始化。普通往返保留正式血量；死亡重试才恢复满血满耐力。
- `ArenaActors.*`：实际成功生成后才加入存活列表；失败的生成名额保留重试。每波 3–4 名，近战至少 1 名、远程 1–2 名。最后一次死亡的经验／掉落回调结束后保存，再等待 3 秒；保存失败时暂停开下一波，并每 5 秒重试。
- `ArenaGameMode.*`：关闭原击杀通关，提供波次／倒计时 HUD，Boss 死亡后保存正式奖励；离场前再次保存已拾取物品。
- `Components/CombatComponent.cpp`：物理落地与蒙太奇推进分别处理；Start、Descent 顺序播放，ContactWait 只负责等接地，随后 Land 收招。落地伤害窗口必须同时满足真实落地和原动画通知，不恢复主角 C++ 自动特效，不改伤害／范围／播放速度。

## 局部地图工具

入口脚本为 `Source/ThirdPerson/Arena/Scripts/build_expansion.py`，调用 `UArenaAssetTools::BuildArenaExpansion()`。需要修改二进制地图时，先保存并关闭 UE，再使用本工程 Editor-Cmd 的 `-run=pythonscript -script=<脚本绝对路径>` 运行。

工具只新建缺失的随机地图，给已有地图增补标记为 `ArenaExpansion_v1` 的对象；外观修正使用 `ArenaPresentation_v2`，教程入口位置修正使用 `ArenaHubPlacement_v3`。重复运行不会重建整个教程／Boss 场，也不会重建攻击蒙太奇。`inspect_expansion.py` 仅检查资源几何和场景信息。

## 验证记录

- **编译**：UE 5.8.2，Win64 Development Editor 和 Win64 Development Game 均成功。初次编译发现的 TObjectPtr 遍历声明、天空组件头文件问题已修正。
- **地图**：局部生成、最终保存及地图／入口／导航检查通过，日志为 `Saved/Logs/ArenaExpansionBuildFinal.log`。
- **最终回归**：31 项全部成功，0 失败、0 未运行；其中 9 项无警告、22 项带日志警告。结果为 `Saved/ArenaExpansion/FinalTests/index.json`，完整日志为 `Saved/Logs/ArenaExpansionFinalTests.log`。
- **跨图实际 PIE**：教程→Boss→教程→随机场，完成三波实际混合敌人、保存经验／拾取物，死亡后通过重试流程回当前场，再经教程返回 Boss；验证完整新挑战、训练装备、课程计数、正式血量／背包和旧检查点不串图。使用一次性测试槽，不写入玩家 `PlayerSave`。
- **边界**：检查 Boss 场地四角、跳跃高度、长时间未见目标、短暂越界返回、持续越界超过 2 秒，以及不绑定边界时旧脱战策略。另通过旧 Boss 导航／遮挡／重置、技能取消和对象池测试。
- **下砸**：30／60／120 FPS 固定时间步 PIE；每档覆盖 2、50、250、1100 厘米起始离地高度、斜坡、台阶和正常取消，共 21 个播放案例；另通过各帧率敌人头顶接触、教程空中命中及落地伤害安全测试。轨迹在 `Saved/ArenaExpansion/dive_playback_30.csv`、`60.csv`、`120.csv`（后两项同样带 `dive_playback_` 前缀）。
- **其他回归**：投射物碰撞／单次伤害／复用、死亡重生和升级保留、药品使用、完整教程、闪避目标与教程存档隔离均通过。
- **视觉**：D3D 离屏 PIE 捕获并人工检查 `Saved/ArenaExpansion/Captures/Tutorial.png`、`Boss.png`、`Random.png`。这只验证实际渲染及布局，不代表已完成真人全流程手感验收或 GPU 性能基准。

## 已知限制与未完成检查

- 另行执行的旧 `ThirdPerson.Sword.AssetContract` 有 3 个断言失败：第四段攻击的提前衔接点、衔接数量及合法后续动作，与当前未提交的第四段连招循环配置不一致。本轮没有修改这些资产或把用户连招改回旧“末段不可衔接”规则；相关日志在 `Saved/Logs/ArenaExpansionTests3.log`。最终 31 项是本轮相关回归集合，不应表述为全项目测试均通过。
- 自动化日志包含 CrowdManager 创建／世界清理时找不到 RecastNavMesh、临时世界销毁、缺少初始存档、现有 Warning 级死亡日志及渲染线程 CVar 警告；地图导航与真实波次生成测试通过，但未在本轮清理引擎和既有日志告警。
- 30／60／120 FPS 是固定模拟步长验证，不是显卡稳定帧率测量。未执行完整 Cook／Package、GPU 基准或磁盘写满／文件权限故障注入；保存失败阻止传送的路径已实现，非法目的地图拒绝及重复交互已自动化验证。
- 场地为已有素材搭建的可玩版本，入口文字使用模型上方英文和交互提示中文；后续美术细化可直接调整新增标记对象，不必再次运行构建工具。
