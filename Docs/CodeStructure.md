# ThirdPerson 代码阅读与安全清理

本项目只有一个 `ThirdPerson` 运行时模块，下列 18 个目录按功能分工，并非 18 个独立的 Unreal 模块。先看类声明上的中文职责说明，再沿关键流程分区阅读实现。

## 功能导航

| 目录 | 负责什么 | 建议入口 |
| --- | --- | --- |
| Actions | 招式、动作集合与编辑器配置 | UActionDefinition、UActionSet |
| AI | 感知、行为树、近战攻击名额与远程后撤 | AEnemyCharacter、AEnemyAIController、UMeleeAICombatSubsystem |
| Animation | 动画状态、动作通知与刀刃补采样 | UTPCAnimInstance、UAttackWindowNotifyState、TPCBladeSampling |
| Arena | 场地边界、传送、随机波次和状态交接 | UArenaTravelSubsystem、AArenaWaveDirector |
| Audio | 脚步和战斗声音的选择、去重与播放 | UTPCCharacterAudioComponent |
| Boss | Countess 遭遇、选招、对峙、破韧和阶段 | UBossActionComponent、UBossDefinition |
| Character | 主角装配、输入、移动、相机和界面协调 | ATPCCharacter、ATPCPlayerController |
| Components | 动作、战斗、生命、耐力、装备、背包和等级 | UActionComponent、UCombatComponent、UHealthComponent |
| GameMode | 普通关卡生成和胜负规则 | ATPCGameMode |
| Interfaces | 可交互对象的共用约定 | IInteractable |
| Items | 物品定义和世界拾取实例 | UItemDefinition、APickupActor |
| NPC | 初始武器选择 | AStarterWeaponNPC |
| Save | 正式进度、检查点和重生交接 | TPCPlayerProgress、UTPCSaveGame |
| Tests | 自动注册的逻辑、资产和 PIE 回归 | 按测试文件顶部中文说明选用例 |
| Tutorial | 课程、真实事件观察、陪练和训练区域 | ATutorialDirector、FTutorialProgress |
| UI | 菜单、HUD、背包分页、属性和角色展示 | UInventoryWidget、UMainMenuWidget |
| Weapons | 武器实例、定义、投射物池与可选特效 | AWeaponActor、UWeaponDefinition、UProjectilePoolSubsystem |
| World | 门、普通通关出口和宝箱 | ADoorActor、ALevelExitActor、ATreasureChestActor |

## 最容易混淆的调用关系

- **攻击时序**：输入进入角色 → 动作组件判断权限与缓存 → 战斗组件播放攻击 → 动画通知打开命中窗口 → 战斗组件扫掠与去重 → 生命组件结算。通知和结束回调都要验证所属动作／蒙太奇实例。
- **伤害配置**：有可用武器时读取武器基础伤害，否则使用战斗组件基础值；加上等级加成，再乘永久倍率和当前 Buff。近战命中再应用当前招式倍率，目标生命组件处理防御、免疫与实际扣血。属性页复用有效基础伤害，不计单招倍率或目标减伤。
- **命中慢动作**：生命结算结果返回后才广播有效近战命中；反馈组件按真实时间恢复速度，同一命中组不重复延长。
- **下砸**：真实触地与动画进入砸地阶段是两件事。低空结束滞空循环，不跳过起手和翻转；高空动画需要等待真实接地，不能由计时器伪造落地。
- **Boss**：角色负责装配，动作组件拥有遭遇状态与技能生命周期。绑定边界后，场内距离或遮挡不直接导致脱战，但攻击仍检查视线。取消、死亡和重置必须清理当前动作及投射物。
- **存档与传送**：正式角色进度和教程训练状态分开；先校验目的地图并保存，再交接抵达标签。旧世界的敌人、动作编号和计时器不能跨地图保留。普通检查点坐标只属于其地图。
- **背包界面**：WidgetSwitcher 仅切背包区域，HUD 和教程提示在外部。属性页低频读取快照；预览只生成展示组件，关闭背包即停止捕获并销毁展示对象。

## 本轮实际修改

1. 记录现有源码差异，并在修改前运行 Editor 编译及相同的 39 项回归测试。
2. 为 124 个类／主要结构体补充或整理中文职责说明；补充关键接口、参数与 42 处流程分区。测试文件和配套脚本增加用途及写入范围说明。
3. 删除 `WeaponDefinition.cpp`、`ItemDefinition.cpp`、`Interactable.cpp`、`TPCSaveGame.cpp` 四个只有 include／模板注释的空实现。对应头文件、反射类型及接口均保留，文件可从 Git 历史找回。
4. 删除模板占位说明、构建文件中注释掉的示例和无用 C# using。检查点头文件移除门与世界遍历依赖，检查点实现移除已不直接使用的五个组件／物品包含；投射物池头文件用命中规格前置声明，完整定义留在实现；移除未使用的 Niagara 前置声明。
5. 保留原有行尾和未提交修改，不统一重排全部源码，不移动函数、成员或目录，不重建资源。

### 明确保留的代码

- UFUNCTION／UPROPERTY、动画通知、委托回调和测试注册不能通过普通 C++ 调用次数判断是否无用。
- 旧存档默认值、旧蒙太奇数组及 Start／Loop／Land 回退仍保留。
- 自动武器特效组件仍被武器、通知和测试引用；主角默认关闭，继续使用动画自带特效。
- `ComboResetTime`、`DashStrength`、`UppercutStunFallbackDuration` 当前原生代码不读取，但保留蓝图序列化字段并注明，避免丢失原配置。
- 编辑器资源工具保留且未执行。尤其旧输入迁移可能覆盖手工映射；当前按键以实际输入资产为准：空格跳跃，Shift 点按闪避／长按快跑。
- 不修正 `InventroyWidget` 等已有文件拼写，以免扩大蓝图引用迁移范围；不修改插件或存档格式。

## 验证记录

- 修改前 Editor 编译通过。
- 基线 39 项：24 项无警告通过、14 项带既有警告通过、1 项失败。失败为 `ThirdPerson.Sword.AssetContract` 的三条既有断言：收招前衔接点、末段衔接点数量、后继动作数量。保留当前用户配置，不修改动画或连招以迎合旧断言。
- 修改后 Editor 和 Game 的 Development／Win64 非 Unity 编译均通过；UHT 头文件解析通过，所有项目实现独立编译并完成链接。
- 同组 39 项回归：24 项无警告通过、14 项带警告通过、1 项失败；失败测试和三条断言与基线完全相同，没有新增失败。包含动作与伤害、完整下砸、Boss 规则、近战／远程资产、投射物、竞技场往返、教程进度、菜单／背包运行逻辑和存档重生。
- 8 个配套 Python 脚本的语法编译通过；只编译语法，没有执行资源写入入口。
- 静态对比确认：除头文件依赖和两处前置声明调整外，运行时代码的非注释内容保持不变。暂存差异检查通过；原有 6 个源码修改单独保留，未提交任何地图、蓝图或其他二进制资产。
- 本轮没有重新做人工画面观感验收；修改不涉及模型、动画内容、UI 布局或特效参数。无新发现的阻塞项，旧剑术资产断言留待专项核对当前连招设计。
- 未执行素材重建、资源自动保存或正式玩家存档清理。本地检查日志位于 `Saved/Logs/CodeCleanup_*`，自动化明细位于 `Saved/Automation/CodeCleanup_*`，不纳入源码提交。
