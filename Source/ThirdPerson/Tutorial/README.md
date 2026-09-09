# 森林遗迹教学关卡

在 UE 内容浏览器打开 `/Game/Third/Tutorial/Maps/L_ForestTutorial`，点击 Play。
原启动地图和原主地图布局保持不变。

关卡使用 `ATutorialGameMode` 和已放置的 `Tutorial_Director`。课程提示、目标类型、次数
及目标 ID 位于 `/Game/Third/Tutorial/Data/DA_ForestTutorial`；区域、起点、靶位和门是
地图中可编辑的 Actor。流程不依赖关卡蓝图。

基础课程按移动、剑术、闪避、防御、血瓶、综合演练依次开放，之后可以自由重练并进入
可选空连场。Esc 菜单支持继续、重试、跳过、重新教学和返回原地图。完成与跳过分别记录。
按键复用现有角色：WASD 移动，长按空格疾跑、短按空格闪避，F 跳跃，左键攻击，
右键格挡/弹反，Q 血瓶，中键锁定，C + 左键升龙，空中 R 下砸。

训练武器与正式武器分离，玩家伤害 20、陪练伤害 8；训练敌人不掉落、不提供经验。
训练剑额外启用 1.2 米下砸落地冲击，由真实落地、动画命中窗和有遮挡检查的碰撞查询触发，
同次落地不会重复伤害。普通武器该参数默认为 0，保持原有剑刃判定。
教学 GameMode 关闭正式存档读写与胜利计数，默认 GameMode 的行为不变。
本节重试、死亡或越界都会重建角色并恢复本节起点/目标/补给，只保留本次会话的前置进度。
退出再进入教学会开启新会话。

## 构建与验证

UE 5.8 Editor 编译后，可用 PythonScript commandlet 执行本目录的
`Scripts/build_map.py`。构建器只创建缺失地图；已有地图仅校验，不覆盖设计者编辑。
运行中脚本不必随游戏分发，关卡内几何、碰撞、光照和导航均已保存。

自动化过滤器 `ThirdPerson.Tutorial` 包括：

- `ProgressAndReplay`：推进、错误事件、去重、跳过、重试、自由练习。
- `PolicyAndObservationEvents`：默认存档策略及成功动作/消耗品只读事件。
- `LandingImpactSafety`：落地/命中窗门控、同次伤害去重、遮挡和距离限制。
- `PIE.ActualCourse`：在保存的地图上用实际输入跑通六节基础与空连挑战，不注入完成事件。
- `PIE.AirHitAtFixedRate`：30、60、120 FPS 下通过真实空连及下砸命中。
- `PIE.SmokeAndSaveIsolation`：暂停、背包、血瓶、死亡重生、越界、临时存档隔离与重复进入。

PIE 测试使用独立随机命名临时存档，结束后恢复原命令行并清除该测试存档。
添加 `-TutorialVisualAudit -RenderOffscreen` 可输出实际 1920×1080 / 1280×720 HUD 截图，
存放在 `Saved/ForestTutorial/Captures`。测试窗口尺寸独立于桌面分辨率。
定向 Cook 地图名为 `/Game/Third/Tutorial/Maps/L_ForestTutorial`。
