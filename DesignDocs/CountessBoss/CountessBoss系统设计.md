# Countess Boss 系统设计 v1

更新：2026-09-07。项目：ThirdPerson，UE 5.8。

**设计定位：双阶段双刀决斗 Boss——「绯刃伯爵夫人」。复用现有敌人的血量、战斗接收、阵营、锁定和死亡结算，新增技能执行、韧性与阶段决策。**

本次交付为设计和资源核对结果；下文的类、资产、技能数值与命中窗口是待实施规格，不代表已经在游戏中实现或通过战斗测试。

## 1. 已确认的选择与资产事实

### 1.1 设计选择

- 已确认：双阶段近战决斗；普通命中削韧、完美弹反大量削韧、破韧眩晕；升龙不直接击飞 Boss。
- 首版默认：行为树负责决策、C++ 负责执行；Boss 直接放入关卡，靠近且可见时开战，脱战回出生点重置。
- 单机、单 Boss、无召唤小怪；不更改现有关卡胜利条件，不重做主角相机、闪避或背包系统。
- 保留 Paragon 原资源。新 Boss 不引用旧 `CountessPlayerCharacter`，也不复制其玩家输入、相机、VR 节点。

### 1.2 实际检查

资源目录：`E:/UNREAL/ue projects/ThirdPerson/Content/ParagonCountess/Characters/Heroes/Countess/Animations`。

对应 UE 路径：`/Game/ParagonCountess/Characters/Heroes/Countess/Animations`。下文短名称均指该目录里的资产名。

| 检查项 | 结果与设计影响 |
|---|---|
| 总计 | 249 个资产：234 个 AnimSequence、4 个 Montage、9 个 BlendSpace 类资产、2 个 AimOffsetBlendSpace |
| Sequence 类型 | 159 个非叠加、39 个 Local Space Additive、36 个 Mesh Space Additive；以资产实际 Additive 属性为准，而非 `_MSA` 后缀 |
| 骨架 | 所选动作使用 `S_Countess_Skeleton`；Boss 使用 Countess 自己的模型和骨架，无需先重定向到主角骨架 |
| 根轨迹 | 对每个 Sequence 取 21 个根变换样本，所有 Sequence 的采样根 XY 轨迹长度均为 0；所选攻击没有可直接驱动突进的根位移 |
| 普攻 | `Primary_Attack_A_Normal / B_Normal / Normal` 均为 0.9 秒；对应 Slow 变体为 1.5 秒 |
| 快攻 | `Primary_Attack_A_Fast_V1 / Fast_V1` 为 0.6 秒，`B_Fast_InMotion` 为 0.4 秒；首版不直接用它们堆攻速 |
| 技能 | Q 0.9667 秒；E 1.1667 秒；RMB 1.3333 秒；Ultimate 3.1667 秒 |
| 受击 | 四向 `Hitreact_*` 都是 Local Space Additive；作为轻微受击叠加，而非普通全身 Sequence 播放 |
| 眩晕与死亡 | `Stun_Start` 1.3333 秒、`Stun_Loop` 1 秒、`Death` 1.6667 秒 |
| 现有 Montage | 三个普通攻击 Montage 使用 `UpperBody` 槽，带 `SaveAttack / ResetCombo` 旧通知；不是本项目现成的 Boss 命中逻辑 |
| 定姿资源 | `Ability_Q_target / Ability_E_target / Ability_R_target / Bound` 只有 1 帧，是姿态资源，不是完整招式 |

已对 9 个核心动作提取 81 组姿态，见 [关键姿态核对图](E:/UNREAL/ue%20projects/ThirdPerson/DesignDocs/CountessBoss/Countess_关键姿态核对.png)。这是骨骼取样图，不是带网格、特效和碰撞的游戏预览。具体刀刃命中帧仍需在实施阶段做网格预览校准。

## 2. 战斗体验、招式和阶段

### 2.1 基础数值与节奏

以下为第一轮可玩原型的起始值，不是对现有 Boss 的实测平衡结果。

| 参数 | 初始值 |
|---|---:|
| 最大生命 | 1500 |
| 第二阶段阈值 | 生命 ≤ 50%，每次遭遇只触发一次 |
| 最大韧性 | 第一阶段 100，第二阶段 120 |
| 普通追击速度 | 400 cm/s |
| 绕行 / 后退速度 | 180 / 220 cm/s |
| 第二阶段追击速度 | 450 cm/s |
| 常用站位距离 | 与主角中心距离 180–260 cm |
| 开战距离 | 900 cm，需有视线；受到玩家有效攻击也可开战 |
| 活动边界 | 以出生点为中心 1600 cm |
| 失去视线容忍 | 5 秒；不会一遮挡就清空目标 |
| 击败经验 | 300，仅结算一次 |

战斗循环：**观察／绕行 → 明确预警 → 承诺出招方向 → 命中窗口 → 可惩罚后摇**。

普通出刀是白色短闪；血术是红色形状预警。预警形状显示范围，颜色只作辅助。主角保持现有锁定相机，不在 Boss 技能里修改玩家的控制旋转或 ViewTarget。

### 2.2 六个招式

下表所有距离为 cm，距离门槛按角色胶囊中心的水平距离判断。伤害为未格挡的基础值。前置预警发生在攻击 Sequence 播放之前，期间停步、保持战斗待机并显示武器/范围预警；命中时刻另见下一表。

| ID / 招式 | 使用动画 | 可选距离 | 第一阶段 / 第二阶段 | 伤害 | 前置预警 / 完整动作后的额外后摇 | 冷却 |
|---|---|---:|---|---:|---|---:|
| Combo / 绯刃连击 | A_Normal → B_Normal；二阶段追加 Normal | ≤ 220 | 两段 / 三段；每段完整播放，段间停顿 0.35 秒 | 16、18、第三段 22 | 第一段前 0.40 秒 / 全套结束 0.55 秒 | 2.5 秒 |
| DelayedSlash / 蓄势重斩 | Primary_Attack_A_Slow | ≤ 240 | 两阶段可用；延迟节奏保持不变 | 30 | 0.55 秒 / 0.55 秒 | 5 秒 |
| Siphon / 环刃汲血 | Ability_Q | ≤ 260 | 两阶段可用；惩罚长期贴身或绕背 | 22 | 圆形预警 0.65 秒 / 0.65 秒 | 9 秒 |
| ShadowRush / 影袭斩 | Ability_RMB | 300–650 | 一阶段单次；二阶段之后可接一刀 Normal | 24，追加刀 18 | 直线预警 0.55 秒 / 单次结束 0.60 秒 | 8 秒 |
| BloodWave / 暗潮血刃 | Ability_E | 400–1200 | 两阶段可用；二阶段可在远距多选此招 | 20 | 直线预警 0.55 秒 / 0.55 秒 | 7 秒 |
| BloodFeast / 血宴跃斩 | Ability_Ultimate | ≤ 350 | 仅第二阶段 | 38 | 圆形预警 0.90 秒 / 1.0 秒 | 18 秒 |

说明：

- 连击每段使用独立命中 ID；两把刀接触同一个目标，不会让单段伤害翻倍。第一阶段不会随机偷偷补第三刀。
- 影袭是**有路径和碰撞的短突进**，不是瞬移到玩家背后。最大位移 500，目标停在玩家前约 140；二阶段追加刀之前再给 0.35 秒预警，且主角仍在 240 范围内才追加。
- 环刃以 Boss 为中心做半径 260 的短时环形扫击；只命中同高度范围，垂直差上限 140，且需无遮挡。命中后回复“实际扣除主角生命的 50%”，单次最多回复 20。
- 血刃是非追踪投射物：速度 900、有效行程 1200、碰到世界阻挡或角色即结束；不使用箭矢的插身/插地保留逻辑。
- 血宴根据原动画的跃起与落地姿态设计成落地血爆：预警半径 330、命中半径 300。首版是动画表现的跃斩，不对主角强制抓取、搬运或切镜头，也不把 Boss 切成飞行模式。
- 普攻、重斩、影袭可格挡且可完美弹反；环刃、血刃可普通格挡但不可完美弹反；血宴忽略格挡、仍服从主角闪避无敌。
- 环刃/血刃遇到右键的短弹反窗口时，按普通格挡结算、正常扣精力，而不是产生错误的完美弹反。
- 未破韧时，完美弹反中断当前物理招式及其后续连段，并给 0.30 秒短停。该短停不是完整眩晕；破韧才进入长时间输出窗口。

### 2.3 Montage 通知配置初值

**时间从对应 Sequence 开始播放时计，不包含前置预警；下列是设计初值，不是原素材已有的通知。** 原素材中所选完整攻击 Sequence 没有项目伤害通知，需要在新 Boss Montage 中添加。

| 动作 | 命中 / 事件初值 | 朝向和移动约束 |
|---|---|---|
| A_Normal | 0.15–0.37 秒，两刀共用一个段命中 ID | 0.10 秒锁定朝向，之后不追转 |
| B_Normal | 0.13–0.40 秒，重新开始一段命中 ID | 同上 |
| Normal | 0.15–0.40 秒，重新开始一段命中 ID | 同上 |
| A_Slow | 0.25–0.62 秒 | 0.18 秒锁定朝向 |
| Ability_Q | 0.10–0.40 秒执行一次短时环形命中窗口 | 执行期位置固定；预警中心跟随 Boss，动作开始后固定 |
| Ability_RMB | 0.12–0.42 秒执行突进，0.43–0.75 秒开刀刃命中窗口 | 突进方向在移动开始前锁定；撞墙立即结束位移 |
| Ability_E | 0.45 秒释放一发血刃 | 释放前 0.15 秒锁定方向；发射后不跟踪 |
| Ability_Ultimate | 2.05–2.25 秒检查一次落地血爆 | 圆圈从预警开始显示；不在命中前瞬移预警或拉扯玩家 |

实施时先在 UE 里显示双刀端点和伤害窗口，以真实刀刃接触校准这些初值，再固化为 Montage Notify。验收要求：视觉接触与伤害开启误差不超过 1 个源动画帧；低帧率也不漏过窗口。

### 2.4 第二阶段与韧性

- 生命首次降到 50% 时设置待转阶段；已有出招先结束。若同次命中刚好破韧，先完整兑现破韧输出窗口，再转阶段。
- 转阶段：停步、取消技能等待队列，使用 `Cast` 加血色特效，总计 1.5 秒；该期间免疫伤害、没有攻击判定。死亡优先于任何转阶段请求。
- 进入第二阶段后：韧性重置为 120，开启血宴、三连斩、影袭后追加刀；追击速度升到 450。原有物理动作播放倍率保持 1.0，伤害不全局翻倍。
- 吸血回复到 50% 以上也不会退回第一阶段。

| 对 Boss 的事件 | 削韧 / 响应 |
|---|---|
| 普通地面命中 | 10；轻微方向性 Additive 受击，不停当前技能 |
| 普通空中命中 | 12 |
| 升龙命中 | 30；只算一次总削韧，不额外叠加普通命中的 10；不 LaunchCharacter |
| 下砸命中 | 25 |
| 完美弹反 Boss 物理招式 | 40；同时中断当前物理招式 |
| 5 秒未受到有效削韧 | 每秒回复 12 韧性，上限为当前阶段最大值 |
| 韧性耗尽 | `Stun_Start → Stun_Loop`，总眩晕 2.4 秒；期间继续受生命伤害，但不刷新眩晕计时 |

破韧结束将韧性补满，恢复期 0.8 秒仅免疫削韧、仍可受到生命伤害，避免退出眩晕的同一瞬间被再次锁死。眩晕不是倒地，不使用 `Knock_*` 冒充起身动作。

## 3. 复用现有模板的技术结构

### 3.1 类与资源职责

```text
AEnemyCharacter
└─ ACountessBossCharacter         Boss 生命周期、阶段、韧性与模板扩展
   ├─ UHealthComponent           复用唯一的生命来源
   ├─ UCombatComponent           复用伤害接收/格挡兼容接口
   └─ UBossActionComponent       新增：技能执行、双刀命中、预警、位移、取消

AEnemyAIController
└─ ACountessBossAIController      感知和目标维持
   └─ BT_CountessBoss / BB_CountessBoss

UCountessBossAnimInstance
└─ ABP_CountessBoss               新动画图，不依赖旧玩家蓝图

UBossDefinition → DA_CountessBoss 招式、冷却、权重、数值与资源
BP_CountessBoss                  数据配置层，Event Graph 保持空白
WBP_BossStatus                   名称、生命、韧性、阶段提示
```

不为 Boss 再造一套独立血量，不让通用 `CombatComponent::TryAttack()` 与 `UBossActionComponent` 同时发起攻击。Boss 招式的唯一执行入口是 BossAction；旧组件只承担兼容功能。

### 3.2 现有代码的必要扩展

1. **敌人模板**：把 `ApplyParryStagger`、`ApplyUppercutHit`、`HandleHealthChanged`、`HandleDeath` 设计为可重写的 C++ 入口。Boss 覆盖普通受击/击飞策略；死亡先取消自己的动作，然后只调用一次父类死亡清理与奖励。默认小怪实现保持现状。
2. **感知模板**：将 `HandleTargetPerceptionUpdated` 改为可重写入口。Boss 丢失视线后保留目标并计时，5 秒后才返回；替换模板“一失去视线立即 Clear TargetActor”的行为。
3. **伤害结果**：增加带上下文的命中入口和结果；保留现有 `ApplyDamage(float)`、`ApplyDamageFrom(float,AActor*)` 作为兼容包装。结果至少区分忽略、普通命中、格挡、完美弹反、致死，并返回实际扣血量。
4. **命中上下文**：包含基础伤害、总削韧、来源 Actor、命中位置、是否可格挡/弹反、攻击实例 ID 与命中窗口 ID。现有小怪和箭矢通过兼容包装维持原规则；主角近战入口为普通/空中/升龙/下砸填写正确的总削韧。
5. **投射物复用**：血刃继续使用现有按类区分的投射物池；为投射物基类增加子类可配置的命中结束策略。普通箭矢默认仍插身/插地保留 5 秒，血刃命中立即回收。获取时先完整设置伤害上下文与来源，再激活碰撞；回收清空本次状态。

拟新增的最小接口契约如下，方法名用于实施时统一调用，以下不是已实现的 C++ 定义：

```cpp
// UHealthComponent：复用血量与现有通知，不另建 BossHealth。
FCombatHitResult ApplyCombatHit(const FCombatHitSpec& Hit, AActor* DamageSource);

// UBossActionComponent：每个实例同时只拥有一个动作。
bool TryStartAction(FName ActionId, AActor* Target);
void CancelAction(EBossCancelReason Reason);
bool IsActionActive() const;

// ACountessBossCharacter：暴露给 Controller、AnimInstance 和 UI。
void StartEncounter(AActor* Target);
void RequestEncounterReset();
EBossPhase GetBossPhase() const;
float GetPoisePercent() const;
```

`UBossDefinition` 存储上述六招对应的 Montage、距离、冷却、阶段权重、预警、伤害属性及位移参数。命中窗口由 Montage 通知触发，数据资产不再保存第二份可独立生效的命中时间表。

### 3.3 命中、位移和中断的关键规则

- 双刀沿上一帧到当前帧的刀刃位置做多点 Sphere Sweep；左右刀分别记录轨迹，同一窗口共用命中集合。查询忽略自己、装备及同阵营敌人，不用物理推挤作为伤害判断。
- 创建仅用于 Boss 的网格副本，在 `weapon_l / weapon_r` 上设置四个 Mesh Socket：`BladeBase_L / BladeTip_L / BladeBase_R / BladeTip_R`。端点按实际刀刃校准；保持原 Skeleton 和动画引用，不修改 Paragon 共享骨架。
- 伤害通过 HealthComponent 一次性结算；闪避无敌和死亡状态先过滤，随后判定格挡/弹反，再结算实际生命和削韧。回血只根据返回的实际扣血值计算；无敌、弹反、打空都不回血。
- `ApplyUppercutHit` 的 Boss 覆盖不再发起位移或二次削韧；升龙的 30 削韧已经来自本次上下文。小怪仍执行原击飞逻辑。
- 中断、致死、失去目标、脱战、地图卸载必须统一关闭伤害窗口、停止刀光、销毁预警、清掉待发射事件，并撤销当前技能拥有的位移源。
- 每次动作分配递增的 `ActionSerial`；Notify 和 Montage 结束回调还要校验 Montage 实例 ID。旧动作的回调不得关闭新动作或补发伤害。
- 技能执行中禁止行为树 MoveTo 和 RVO 修改招式位移；保存并在动作结束时恢复原移动/转向设置。所有位移只作用于 CharacterMovement 驱动的角色胶囊，不直接移动 Mesh。
- 影袭使用 CMC 的 `FRootMotionSource_MoveToForce`，保存返回 ID，结束只撤销自身源。地面目标先做 NavMesh 与胶囊路径检查；目的地不可达、路径被墙挡或会离开地面时，取消该次突进并进入后摇，冷却照常开始。
- 动作开始后的普通攻击不吸附、不持续转追玩家；方向锁定前最多 120°/s 转向。非攻击站位转向最多 240°/s。

根运动源可由代码向 CharacterMovement 注入移动，并通过返回句柄管理；它不同于从动画文件提取根位移。本设计使用这一机制执行影袭，不要求原动画拥有根运动。[Epic：Character Movement 与 Root Motion Sources](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-networked-movement-in-the-character-movement-component-for-unreal-engine)

## 4. 行为树、动画图与 UE 配置

### 4.1 行为树结构

优先级从上到下。死亡和重置能够中断任何节点；破韧先于待转阶段；普通的距离变化不随意取消已经承诺的招式。

```text
Root / Selector
├─ Dead                         → 停脑、清理、等待销毁
├─ ResetRequested               → 取消技能 → 回出生点 → 重置
├─ !EncounterActive             → 留在出生点待机，等待开战请求
├─ Intro                        → 播放入场 → 开战
├─ PoiseBroken                  → Stun_Start → Stun_Loop → 恢复
├─ PhasePending && !ActionActive→ 转阶段
├─ ActionActive                 → 等待当前动作 / 后摇结束
└─ Combat / Selector
   ├─ NeedsSeparation           → 非攻击期复用分离位置计算
   ├─ HasValidAction           → 选招 → 执行
   ├─ TooClose                 → 面朝玩家后退
   ├─ InOrbitRange             → 左右绕行与短等待
   └─ Otherwise                → 追击最后已知位置
```

- `UpdateBossContext` Service 每 0.10 秒更新目标、视线、距离和重置条件；不每 Tick 全场搜索 Actor。
- Blackboard 保留模板的 `TargetActor / HomeLocation / LastKnownLocation / bHasLineOfSight / bIsDead / bIsStunned`；增加 `bEncounterActive / BossPhase / bActionActive / bPhasePending / bResetRequested / SelectedAction / MoveGoal`。只有合法 StartEncounter 请求才将 bEncounterActive 设为 true；无目标待机时不执行追击分支。
- 技能只在未冷却、当前阶段允许、距离满足、目标有效且有视线时进入候选。按固定种子的加权随机选取，便于复现测试。
- 一／二阶段权重分别为：Combo 45/35，DelayedSlash 20/20，Siphon 15/15，ShadowRush 10/15，BloodWave 10/15，BloodFeast 0/20。仅在合法候选之间归一化；连续使用同一特殊技能后的下一次选择排除它，Combo 不受此排除。
- 冷却从动作接受时开始；每次取消也保留冷却。无合法招式时采用绕行或追击，不为了攻击而忽略距离、视线或冷却。
- 保持第一阶段每套动作结束后至少 0.55 秒可输出窗口；第二阶段至少 0.45 秒。血宴完整打空仍保留 1 秒额外后摇。
- 近距后退和绕行均 `AllowStrafe=true`，朝向玩家；攻击期间暂停模板的分离服务和 RVO，避免被其抢回移动控制。
- Boss 独立选招，不占用小怪的攻击令牌。首版以单 Boss 测试场景验收，友伤过滤继续生效。

### 4.2 动画图

```text
Locomotion：Idle / 四向移动 / Falling
  → Slot：DefaultGroup.DefaultSlot
  → Apply Additive：四向轻受击（按命中方向选一段，死亡/转阶段/破韧时权重为 0）
  → Inertialization
  → Dead 分支：Death Sequence（不循环，停在末帧）
  → Output Pose
```

- `UCountessBossAnimInstance` 从受控 Pawn 与组件读取 Speed、Direction、IsFalling、BossPhase、ActionState、HitDirection 和死亡状态；不 Cast 到旧玩家 Blueprint。
- 战斗待机用 `Idle_Pose`；非战斗待机用 `Idle_Relaxed`；追击/绕行/后退用新建的四向 BlendSpace，选 `Jog_*_Combat`；远距追击可混合 `Sprint_Fwd`。
- BlendSpace 输入来自角色真实水平速度和局部方向。步频通过脚接地段和移动距离校准；该素材根轨迹为 0，不以“根位移÷时长”估算行走速度。意外离地使用 `Jump_Apex`，落地使用 `Jump_Land` 后返回站立；AI 首版不主动发起空中攻击。
- 正式攻击使用新建的全身 Montage 和 `DefaultSlot`。不直接接旧的三个 `UpperBody` Montage，不保留 `SaveAttack / ResetCombo` 作为 Boss 连招逻辑。
- 普通命中的 `Hitreact_*` 使用 Local Space `Apply Additive`，峰值权重 0.25；受击时允许轻微颤动，不让全身攻击停掉。破韧独立使用全身 Stun Montage。
- 父类 `DeathMontage` 对这个 Boss 配置为空，死亡由动画图最高优先级 `Death` 播放一次并保持末帧；父类仍负责停 AI、停碰撞和生命期清理。避免死亡蒙太奇淡出后尸体重新站立。
- `MinimumDeathLifeSpan=5.0`，让死亡动画和末帧停留完整表现；动画/技能回调均不能在死亡后恢复移动。

### 4.3 资源与特效

新建项目资源统一放在 `/Game/Third/Bosses/Countess`：角色配置、独立 AnimBP、BlendSpace、Boss Montage、数据资产、行为树、黑板、血条和可测场景。网格可复制一份配置 Mesh Socket；动画 Sequence 继续引用原素材。

| 用途 | 优先复用的现有特效资产名 |
|---|---|
| 双刀拖尾 | p_CountessMeleeTrail |
| 真实命中点 | p_CountessImpact |
| 环刃 | P_Countess_BladeSiphon_RingFX |
| 影袭开始 / 到达 | P_Countess_TeleportBegin / P_Countess_TeleportArrive |
| 血刃施法 | P_Countess_RD_CastBurst |
| 血宴 / 转阶段 | p_CountessUlt_CastFX / p_CountessUlt_GroundImpactFX |

这些特效位于 `/Game/ParagonCountess/FX/Particles` 的相应子目录。应按实际 ParticleSystem 类型使用粒子组件；无需为了本 Boss 把它们转换成 Niagara。粒子负责表现，真正命中仍由 C++ 查询结算。刀光跟随四个刀刃 Socket，命中特效用本次 HitResult 的命中点。

范围预警新建简洁的环形/条形材质，地面检测决定其高度与朝向；不会在角色胶囊原点盲目生成全部特效。不接入任何 `CameraLens` 类技能特效到玩家镜头。

### 4.4 遭遇、血条与结算

- 直接将 `BP_CountessBoss` 放入关卡，Auto Possess AI 设置为 Placed in World or Spawned，AI Controller 指向 Boss Controller，配好 NavMesh。
- 当前 `AEnemySpawner` 默认在敌人销毁后 3 秒重生；首版 Boss 不通过它生成，避免胜利后又刷一只。
- 首次开战播放 `LevelStart`，期间 Boss 无伤害判定且免疫伤害，玩家保持控制；入场结束进入第一阶段。一次关卡运行内重战只做 0.75 秒预备，不重复长入场。
- 玩家死亡、离出生点超过 1600 且持续 2 秒，或丢失视线达 5 秒时请求脱战：停止造成伤害、回家、恢复生命/韧性/第一阶段、清理所有技能对象。禁止边回家边射血刃。
- 回家路径连续失败或 8 秒未返回时，仅在已脱战且玩家看不到 Boss 时重放置于验证过的出生点；玩家可见时保持无攻击的回家状态并重试，不当面瞬移。
- 独立 Boss HUD 使用事件更新：顶部居中、设计宽度 720、高度约 64（以 1920×1080 为参考），显示名称、生命条、细韧性条、第二阶段标记；UMG Anchor 为顶部中心，使用项目 DPI 缩放，不重新塞进背包面板。
- Boss 自带的小怪头顶血条隐藏；白色锁定光球继续跟随 `spine_03`。光球位置与相机瞄准参考点保持独立。
- 脱战隐藏 HUD；击败后保留名称与空血条 2 秒再淡出。父类经验、击杀计数只增加一次；Boss 自身不调用全局 WinGame。是否掉额外物品仍使用模板 DropPickupClass/DropChance，首版额外掉落类留空。

## 5. 实施顺序与验收

### 5.1 分四步落地

1. **模板与基础角色**：为模板加可重写策略入口和命中结果；建立 Boss 子类、AnimInstance、独立 AnimBP。先验证站立、四向移动、锁定、受击和死亡。
2. **物理决斗闭环**：双刀 Socket、基础连击、延迟重斩、影袭、韧性与弹反。确认取消路径，再接行为树选招。
3. **血术与第二阶段**：环刃、血刃、血宴、阶段切换与特效；投射物复用同时回归已有箭矢。
4. **关卡闭环**：入场、Boss HUD、脱战重置、奖励与测试场景；再调整手感数值。

### 5.2 必须通过的测试

| 测试场景 | 通过条件 |
|---|---|
| 资源检查 | 新 Boss / AnimBP / Montage / BT 编译成功；没有指向 CountessPlayerCharacter 的新依赖；叠加动画使用正确节点 |
| 命中去重 | 同一刀窗左右刀同时命中、连续多帧接触，都只结算一次；下一段独立结算 |
| 闪避与防御 | 无敌不掉血、不削韧、不吸血；物理招式可弹反；血术的格挡/弹反规则与表一致；背后攻击不被正面格挡错误拦截 |
| 韧性 | 普通攻击/升龙/弹反分别扣 10/30/40；升龙不双扣、不击飞；破韧正好持续 2.4 秒，额外命中不延长 |
| 阶段边界 | 单次跨过 50%、跨阈值同时破韧、跨阈值同时致死、吸血回到 50% 以上均只有正确的一次状态切换 |
| 中断清理 | 出招任意帧被弹反、破韧、击杀或脱战后，旧刀窗、预警、位移源、延期发射与旧 Notify 不继续生效 |
| 位移 / 相机 | 影袭只移动角色胶囊；撞墙、目标背后有墙、台阶、悬崖、锁定状态都不穿透、不把 Mesh 留在胶囊外，也不改变玩家相机控制 |
| AI 导航 | 能靠近开战、绕行、朝向玩家后退；短时遮挡保留目标；路径失败不站着无限挥空；攻击中没有 MoveTo/RVO 抢控制 |
| 死亡 | 死亡同帧关闭伤害，尸体保持死亡末帧，约 5 秒销毁；只计一次经验与击杀；既有小怪死亡/升龙/弹反仍正常 |
| 关卡与投射物 | 重战清理 HUD 和技能对象；Boss 不被普通刷怪器无限重生；血刃命中回收；旧箭矢仍造成伤害、附着后 5 秒回收 |
| 帧率 | 30 / 60 / 120 FPS 下同段命中次数相同；低帧率跨过短窗口仍正确执行一次，不依赖每帧恰好落在某个时间点 |
| 战斗体验 | 第二阶段靠招式变化而非单纯倍率区分；技能打空有输出机会；当前主角的受击锁若覆盖整套连击，优先加大 Boss 段间间隔而非静默改主角规则 |

自动测试覆盖伤害结果、削韧、阶段与回调失效；PIE 场景覆盖双刀接触、动画同步、位移碰撞、HUD 和现有小怪回归。记录 ActionId、ActionSerial、Phase、Poise、命中结果和位移源 ID，调试时可显示在 Gameplay Debugger。

### 5.3 修改与回退边界

实施前保存既有源文件和即将编辑的 UE 资产哈希，先在项目副本中完成模板扩展及回归。原 Paragon 资源不就地改写，新增 Boss 资产集中管理。

提交实施结果时包含变更包、文本差异、验证报告、可执行回退脚本；回退在另一个副本中验证，恢复模板代码与被改资产、移除本次新增 Boss 资源，同时保留其他用户修改。只有实际完成构建和 PIE 后，才把上述验收项标为通过。

本设计附带的资产目录和检查记录仅证明资源读取、分类与设计引用有效；Boss 的运行时验收留给实施阶段，不以“设计完成”替代“功能完成”。
