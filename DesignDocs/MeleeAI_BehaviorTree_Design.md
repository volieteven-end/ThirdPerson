# 近战敌人行为树与多人围攻设计

## 1. 文档目标

本文档为当前第三人称 Roguelike 项目设计第一种持剑近战敌人。设计重点不是让敌人以最短路径持续贴住玩家，而是让敌人表现出可读、可反应、可扩展的战斗行为，并在敌人数量增加时避免所有敌人同时挤向玩家和同时攻击。

当前项目已有能力：

- 敌人具有生命、受击、死亡、巡逻和追击逻辑。
- 敌人具有 `CombatComponent` 与 `EquipmentComponent`。
- 武器攻击可以播放 Montage，并在攻击窗口沿 `BladeBase`、`BladeTip` 检测。
- 玩家具有生命、体力、闪避、等级、背包和存档系统。
- 当前 AI 参数约为 `ChaseDistance = 800`、`StopDistance = 150`。

第一版行为树应优先保证稳定与可读性，然后再增加复杂战术。

---

## 2. 核心体验原则

### 2.1 近战敌人不应一直后退保持远距离

近战敌人的职责是给玩家施加近距离压力。因此它应主动接近玩家，但不能永久贴在玩家胶囊体上。推荐行为节奏：

1. 发现玩家并接近。
2. 进入战斗环后减速并调整角度。
3. 获得攻击机会后向前压进并攻击。
4. 攻击结束后短暂恢复、侧移或后撤。
5. 再次寻找攻击机会。

这种“接近—攻击—恢复—重新接近”的循环比持续贴脸更自然。

### 2.2 玩家必须能看懂攻击

每次攻击至少包含：

- 前摇：敌人抬剑或改变姿势。
- 有效攻击窗口：剑刃检测开启。
- 后摇：攻击结束，敌人暂时不能立即再次攻击。

敌人不能在没有明显动作时直接扣除玩家生命。

### 2.3 多名敌人不能同时获得攻击权

如果六个敌人同时运行“接近玩家并攻击”，它们会一拥而上、胶囊体互相推挤并同时造成伤害。解决方案是将“能够接近”与“能够攻击”分开：

- 所有敌人都可以进入战斗区域。
- 只有少量敌人持有攻击令牌（Attack Token）。
- 没有令牌的敌人在外围包围、侧移和等待。

推荐同时攻击者数量：

| 场上近战敌人数 | 最大攻击令牌 |
|---:|---:|
| 1～2 | 1 |
| 3～5 | 2 |
| 6～8 | 2，困难模式可为3 |

---

## 3. 战斗距离分层

距离均以敌人与玩家 Actor Location 的水平距离为基础，第一版忽略高度差较大的目标。

| 区域 | 推荐距离 | 行为 |
|---|---:|---|
| 脱战区 | 大于1200 | 返回出生区域或巡逻 |
| 警戒区 | 800～1200 | 调查玩家最后已知位置 |
| 追击区 | 320～800 | 快速接近玩家 |
| 战斗环 | 190～320 | 降速、面向、侧移、申请攻击令牌 |
| 攻击区 | 110～190 | 持有令牌时攻击 |
| 过近区 | 小于100 | 后撤、侧移或重新寻路 |

当前 `ChaseDistance = 800` 可以作为视觉发现距离。当前 `StopDistance = 150` 接近攻击距离，但行为树版本建议将“移动停止距离”和“实际攻击距离”分开。

推荐初始参数：

```text
SightRadius              = 900
LoseSightRadius          = 1200
CombatRingMinDistance    = 190
CombatRingMaxDistance    = 300
AttackDistance           = 165
TooCloseDistance         = 95
ReturnHomeDistance       = 1600
AttackFacingTolerance    = 35 degrees
AttackRecoveryTime       = 0.6～1.0 seconds
```

---

## 4. 单个敌人的遭遇流程

### 4.1 巡逻

敌人在设置的 Patrol Points 之间移动。抵达巡逻点后等待随机时间，例如0.8～2秒，再前往下一个巡逻点。随机等待可以避免多个敌人完全同步。

### 4.2 发现玩家

敌人通过 AI Perception 的 Sight 感知玩家：

- 首次看到玩家：设置 `TargetActor`，记录 `LastKnownLocation`。
- 可选：播放警觉动画或停顿0.2秒。
- 向遭遇管理器登记自己进入战斗。
- 开始追击，但尚未立即获得攻击权。

### 4.3 追击

当距离大于战斗环最大距离时，敌人使用 `Move To` 接近玩家。移动目标不应始终是玩家中心，而应逐步切换为玩家周围的包围位置。

追击阶段建议：

- 使用导航移动，而不是每帧直接设置位置。
- 每0.15～0.3秒更新一次移动目标，不必每帧重新 `MoveTo`。
- 玩家快速移动时更新追击点。
- 路径失败时短暂等待，再重新寻路。

### 4.4 进入战斗环

敌人进入190～300距离后：

- 降低移动速度。
- 朝向玩家。
- 选择玩家周围的一个包围槽位。
- 没有攻击令牌时，在槽位附近侧移或做威胁动作。
- 有攻击令牌时，向攻击距离压进。

### 4.5 攻击

敌人只有同时满足以下条件才开始攻击：

```text
TargetActor 有效
目标仍存活
距离 <= AttackDistance
与玩家的夹角 <= AttackFacingTolerance
当前不在受击、死亡或攻击状态
攻击冷却结束
持有 Attack Token
```

开始攻击后：

1. 停止导航移动。
2. 转向玩家。
3. 播放 `AM_EnemySwordAttack`。
4. Damage Notify State 开启剑刃检测。
5. Montage 结束后进入恢复状态。

攻击已经开始后，不应让敌人持续旋转追踪玩家，否则挥剑动作会像自带锁头。推荐只允许前摇初期进行少量朝向修正，进入伤害窗口后锁定方向。

### 4.6 攻击恢复

攻击结束后，敌人进入0.6～1秒恢复期：

- 暂时不允许再次攻击。
- 释放攻击令牌，让其他敌人获得机会。
- 随机选择后撤、侧移或原地防备。
- 恢复结束后重新申请令牌。

### 4.7 玩家离开视野

短暂丢失玩家时：

- 移动到 `LastKnownLocation`。
- 搜索2～4秒。
- 搜索期间缓慢转向或走到附近检查点。
- 重新看见玩家则回到战斗。
- 搜索失败则返回出生区域或巡逻路线。

### 4.8 受击与死亡

受击时高优先级分支打断移动和普通攻击：

- 轻攻击：短受击动画，可中断敌人前摇。
- 敌人进入霸体攻击时，可选择不被轻击打断。
- 死亡：立即释放攻击令牌、停止行为树、关闭碰撞、播放死亡 Montage。

---

## 5. 多敌人包围系统

### 5.1 包围槽位

在玩家周围建立虚拟圆环，并分配若干位置：

```text
                Slot 0

        Slot 7           Slot 1

    Slot 6       Player       Slot 2

        Slot 5           Slot 3

                Slot 4
```

推荐8个槽位，半径220～300。敌人根据以下规则选择槽位：

- 优先选择距离自己最近的空槽位。
- 避免选择已被其他敌人占用的位置。
- 槽位不可导航时尝试相邻槽位。
- 玩家移动较远后重新计算槽位。
- 槽位位置需要投射到 NavMesh。

没有攻击令牌的敌人移动到槽位，而不是移动到玩家中心，因此不会全部堆在同一点。

### 5.2 攻击令牌

建议创建一个遭遇协调器，例如：

```text
ACombatCoordinator
或
UCombatCoordinatorSubsystem
```

协调器负责：

- 登记正在围攻玩家的敌人。
- 分配与释放包围槽位。
- 限制同时拥有攻击令牌的敌人数。
- 防止同一方向连续发动攻击。
- 敌人死亡或离开战斗时回收资源。

敌人申请令牌：

```text
RequestAttackToken(Enemy)
```

以下情况必须释放：

```text
攻击完成
攻击被打断
敌人受击硬直
敌人死亡
敌人距离玩家过远
令牌持有超时
```

建议令牌最大持有时间为2～3秒，避免某个寻路失败的敌人永久占用令牌。

### 5.3 防止同时攻击

仅限制令牌数量仍可能让两名敌人在完全相同的时刻攻击。协调器可以增加全局攻击间隔：

```text
GlobalAttackSpacing = 0.35～0.6 seconds
```

当一名敌人刚开始攻击时，其他持有令牌的敌人至少等待这个时间再开始攻击。这样战斗有压力，但不会出现不可读的同步伤害。

### 5.4 等待中的敌人做什么

等待者不能像木桩一样站着。可随机执行：

- 顺时针或逆时针侧移。
- 向前试探一步再退回。
- 播放持剑防备动作。
- 发出叫声或短促威胁动作。
- 当玩家背对自己时提高获取令牌的优先级。
- 当已有攻击者位于玩家正面时，尝试移动到侧后方。

这些动作只改变表现，不应偷偷造成伤害。

---

## 6. 推荐 Blackboard

创建：

```text
BB_MeleeEnemy
```

推荐键：

| Key | 类型 | 用途 |
|---|---|---|
| `TargetActor` | Object/Actor | 当前玩家目标 |
| `HomeLocation` | Vector | 出生位置或守卫中心 |
| `LastKnownLocation` | Vector | 最后看到玩家的位置 |
| `PatrolTarget` | Object/Actor | 当前巡逻点 |
| `CombatSlotLocation` | Vector | 当前包围槽位 |
| `DistanceToTarget` | Float | 与玩家的水平距离 |
| `bHasLineOfSight` | Bool | 是否能看到玩家 |
| `bHasAttackToken` | Bool | 是否拥有攻击权 |
| `bIsAttacking` | Bool | 是否正在播放攻击 |
| `bIsInAttackRange` | Bool | 是否进入攻击距离 |
| `bIsTooClose` | Bool | 是否与玩家过近 |
| `bIsStunned` | Bool | 是否受击硬直 |
| `bIsDead` | Bool | 是否死亡 |
| `SearchLocation` | Vector | 调查/搜索位置 |

不要把每个临时数值都放进 Blackboard。仅将需要被多个节点共享或被 Decorator 观察的数据放进去。

---

## 7. 推荐 Behavior Tree 结构

```text
Root
└── Priority Selector
    ├── Sequence: Dead
    │   └── Stop Logic / No Operation
    │
    ├── Sequence: Stunned
    │   ├── Stop Movement
    │   └── Wait Until Stun Ends
    │
    ├── Selector: Has Target
    │   ├── Sequence: Too Close
    │   │   ├── Calculate Retreat/Side Position
    │   │   └── Move To Position
    │   │
    │   ├── Sequence: Can Attack
    │   │   ├── Decorator: Has Attack Token
    │   │   ├── Decorator: In Attack Range
    │   │   ├── Decorator: Facing Target
    │   │   ├── Stop Movement
    │   │   ├── Rotate To Face Target
    │   │   ├── Perform Melee Attack
    │   │   ├── Wait Attack Recovery
    │   │   └── Release Attack Token
    │   │
    │   ├── Sequence: Has Attack Token But Too Far
    │   │   ├── Calculate Attack Approach Position
    │   │   └── Move To
    │   │
    │   ├── Sequence: Surround And Wait
    │   │   ├── Request Combat Slot
    │   │   ├── Move To Combat Slot
    │   │   ├── Face Target
    │   │   ├── Request Attack Token
    │   │   └── Strafe/Threaten
    │   │
    │   └── Sequence: Chase
    │       └── Move To Target Region
    │
    ├── Sequence: Investigate Last Known Location
    │   ├── Move To LastKnownLocation
    │   ├── Search Area
    │   └── Clear Target If Not Found
    │
    └── Sequence: Patrol
        ├── Select Patrol Point
        ├── Move To Patrol Point
        └── Wait Random Time
```

高优先级分支应使用 Observer Aborts，让死亡、受击或失去目标能够立即中断低优先级移动任务。

---

## 8. 需要的 Service、Task 与 Decorator

### Services

#### `BTS_UpdateCombatContext`

每0.15～0.25秒更新：

- `DistanceToTarget`
- `bIsInAttackRange`
- `bIsTooClose`
- `bHasLineOfSight`
- `LastKnownLocation`

不要每帧执行昂贵的寻路或环境查询。

#### `BTS_UpdateCombatSlot`

玩家移动超过一定距离、槽位失效或敌人被卡住时，请求新的包围槽位。

### Tasks

```text
BTT_FindPatrolPoint
BTT_RequestCombatSlot
BTT_RequestAttackToken
BTT_ReleaseAttackToken
BTT_RotateToTarget
BTT_PerformMeleeAttack
BTT_SelectRetreatPosition
BTT_StrafeAroundTarget
BTT_SearchLastKnownLocation
```

`BTT_PerformMeleeAttack` 不应在调用 `TryAttack()` 后立即返回成功。它应等待攻击 Montage 结束或攻击完成事件，再结束任务，从而防止行为树同时执行其他移动逻辑。

### Decorators

```text
Target Is Valid
Target Is Alive
Has Attack Token
In Attack Range
Facing Target
Attack Cooldown Ready
Within Home Leash
```

攻击冷却可以用 Cooldown Decorator，也可以继续由 `CombatComponent` 最终校验。行为树 Decorator 用于避免无意义地频繁执行攻击 Task，组件校验负责最终安全性。

---

## 9. 移动与碰撞建议

### 9.1 使用 NavMesh

所有追击、包围、后撤位置都应投射到 NavMesh。无法投射的位置不能作为有效槽位。

### 9.2 群体避让

可以选择：

- `Detour Crowd AI Controller`
- `CharacterMovement` 的 RVO Avoidance

第一版选择一种即可，不要同时依赖两套避让系统，否则移动方向可能互相竞争。当前已有 `UCrowdManager` 相关日志，正式使用前需确认关卡存在有效 `RecastNavMesh`。

### 9.3 不要让胶囊体承担全部战术

胶囊碰撞只能防止模型穿透，不能主动形成好看的包围。包围槽位和攻击令牌才是避免一拥而上的主要机制。

---

## 10. 公平性规则

建议至少遵守以下规则：

1. 镜头外敌人的攻击频率降低，或禁止发动高伤害攻击。
2. 玩家刚受击后的短时间内，其他敌人降低攻击权重。
3. 同一时刻最多1～2个敌人处于有效伤害窗口。
4. 背后攻击应有声音提示。
5. 敌人攻击前摇不能因为距离变化被任意缩短。
6. 敌人攻击落空后仍需承担后摇。
7. 敌人不能隔着墙获取攻击令牌并持续占用。

可选的玩家受击保护：

```text
GlobalPlayerHitGracePeriod = 0.15～0.3 seconds
```

这不是长时间无敌，而是防止同一帧被多个敌人同时命中造成瞬间死亡。

---

## 11. 难度扩展

### 普通近战怪

- 一个普通攻击 Montage。
- 攻击前摇明显。
- 攻击后释放令牌。
- 不格挡，不闪避。

### 精英近战怪

- 两种攻击，根据距离随机选择。
- 偶尔格挡或抗硬直。
- 更高攻击令牌优先级。
- 生命较低时使用冲刺攻击。

### 随玩家等级成长

推荐只进行温和成长：

```text
EnemyMaxHealth = BaseHealth × (1 + PlayerLevel × 0.08)
EnemyDamage    = BaseDamage × (1 + PlayerLevel × 0.05)
```

不要让敌人数值完全同步追上玩家增强，否则升级会失去意义。敌人数量、组合和行为复杂度应比纯数值增长更重要。

---

## 12. 调试可视化

开发阶段建议绘制：

- 黄色圆：战斗环。
- 红色圆：攻击距离。
- 蓝色点：包围槽位。
- 绿色点：当前敌人占用的槽位。
- 头顶文字：当前行为树状态。
- 头顶图标：是否持有攻击令牌。
- 青色线：剑刃攻击轨迹。

建议日志：

```text
Enemy requested attack token
Enemy received attack token
Enemy released attack token: AttackFinished
Enemy released attack token: Stunned
Enemy changed combat slot
Enemy lost target and started investigation
```

---

## 13. 分阶段实现计划

### 阶段一：单敌人行为树

实现：

- AI Perception 发现玩家。
- Blackboard 保存目标与距离。
- Patrol、Chase、Attack、Return Home。
- 攻击 Task 等待 Montage 结束。

验收：一只敌人可以稳定巡逻、发现、追击、攻击、丢失目标并返回。

### 阶段二：基础战斗环

实现：

- 战斗距离分层。
- 过近后撤。
- 攻击后恢复和侧移。

验收：敌人不会永久挤在玩家胶囊体上。

### 阶段三：多人协调

实现：

- Combat Coordinator。
- 8个包围槽位。
- 1～2个攻击令牌。
- 令牌超时与死亡回收。

验收：六只敌人不会全部移动到同一点，不会同时攻击。

### 阶段四：表现与难度

实现：

- 侧移、威胁动作和攻击音效。
- 不同攻击权重。
- 玩家等级对应的温和成长。
- 精英近战怪扩展。

---

## 14. 当前项目的推荐第一版

为了避免一开始写得过于复杂，当前项目第一版采用：

```text
单个敌人：巡逻 → 发现 → 追击 → 攻击 → 恢复
多个敌人：8个包围槽位 + 最多2个攻击令牌
攻击者：进入165距离后攻击
等待者：维持220～300距离并侧移
过近者：小于95距离时后撤
攻击结束：释放令牌并恢复0.8秒
失去目标：搜索3秒后返回巡逻
```

这个版本已经能形成明显的围攻节奏，并为后续弓箭敌人保留空间。远程敌人未来可以占用更外层的远程槽位，不与近战槽位竞争。
