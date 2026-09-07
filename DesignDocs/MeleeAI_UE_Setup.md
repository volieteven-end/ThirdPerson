# 近战 AI：Unreal 行为树配置步骤

代码侧已经实现 AI 感知、战斗位置、攻击令牌、后撤位置、巡逻点和等待攻击动画结束。下面只需要在 Unreal Editor 中建立 Blackboard 与 Behavior Tree 资产。

## 1. 创建资产

在 `Content/Third/AI` 下创建：

1. Blackboard：`BB_MeleeEnemy`
2. Behavior Tree：`BT_MeleeEnemy`，Blackboard 选择 `BB_MeleeEnemy`
3. 如果当前使用 C++ `EnemyAIController`，建议创建蓝图子类 `BP_MeleeAIController`

## 2. Blackboard 字段

名称必须完全一致：

| 名称 | 类型 | 用途 |
|---|---|---|
| `TargetActor` | Object，Base Class 为 Actor | 当前玩家 |
| `HomeLocation` | Vector | 出生位置 |
| `LastKnownLocation` | Vector | 最后看见玩家的位置 |
| `PatrolTarget` | Object，Base Class 为 Actor | 当前巡逻点 |
| `CombatSlotLocation` | Vector | 包围槽位 |
| `RetreatLocation` | Vector | 过近时的后撤点 |
| `DistanceToTarget` | Float | 与玩家的二维距离 |
| `bHasLineOfSight` | Bool | 是否仍能看见玩家 |
| `bHasAttackToken` | Bool | 是否取得攻击资格 |
| `bIsInAttackRange` | Bool | 是否进入 165 攻击距离 |
| `bIsTooClose` | Bool | 是否小于 95 距离 |
| `bIsStunned` | Bool | 预留受控状态 |
| `bIsDead` | Bool | 预留死亡分支 |

## 3. 根节点与 Service

在根 Selector 上添加 `Update Melee Combat Context` Service。默认每 0.15 秒更新距离、视线、攻击范围和过近状态。

根 Selector 从左到右排列，越靠左优先级越高：

```text
Root Selector
├─ Dead
├─ Stunned
├─ Has Target
│  ├─ Too Close
│  ├─ Can Attack
│  ├─ Hold Combat Slot
│  └─ Chase
├─ Investigate Last Known Location
└─ Patrol
```

## 4. Has Target 分支

建立 Sequence，并添加 Blackboard Decorator：

- Key：`TargetActor`
- Query：`Is Set`
- Observer Aborts：`Both`

### 4.1 Too Close

Sequence：

1. Decorator：`bIsTooClose == true`，Observer Aborts 为 `Both`
2. `Select Retreat Position`
3. `Move To`：Blackboard Key 选择 `RetreatLocation`，Acceptable Radius 设为 `25`

### 4.2 Can Attack

Sequence：

1. Decorator：`DistanceToTarget <= 320`，Observer Aborts 设为 `None`
2. `Request Attack Token`
3. `Move To`：Key 选择 `TargetActor`，Acceptable Radius 设为 `20`；保留 Agent/Goal Radius 的到达检测
4. `Perform Melee Attack`（任务会停止移动并直接朝向目标，而且超过 `Maximum Attack Distance=180` 时拒绝攻击）
5. Unreal 自带 `Wait`：`0.8` 秒，Random Deviation `0.15`

`Perform Melee Attack` 会等待攻击 Montage 结束，并在成功、打断或分支 Abort 时释放攻击令牌。没有 Montage 时会执行现有瞬时攻击并立即完成。

### 4.3 Hold Combat Slot

Sequence：

1. `Request Combat Slot`
2. `Move To`：Key 为 `CombatSlotLocation`，Acceptable Radius `35`
3. `Wait`：`0.15` 秒，Random Deviation `0.05`

把该 Sequence 放进一个可重复执行的节点结构。每次重新请求的位置会缓慢绕玩家旋转，因此等待者会侧移，而不是站死或一拥而上。

### 4.4 Chase

`Move To`：Key 选择 `TargetActor`，Acceptable Radius 建议 `240`。该分支只负责从远处接近；进入战斗环后，前面的 Combat Slot 分支会接管。

## 5. 搜索分支

Sequence：

1. Decorator：`TargetActor Is Not Set`
2. Decorator：`LastKnownLocation Is Set`
3. `Move To LastKnownLocation`
4. `Wait 3.0`
5. 项目自定义的 `Clear Blackboard Value`：将 `Key To Clear` 设为 `LastKnownLocation`

## 6. 巡逻分支

Sequence：

1. `Set Current Patrol Point`，第一次不勾选 `Advance First`
2. `Move To PatrolTarget`，Acceptable Radius `50`
3. `Wait 1.0`
4. 再放一个 `Set Current Patrol Point`，勾选 `Advance First`

如果敌人蓝图实例没有填写 `PatrolPoints`，该分支失败后敌人会原地待机。

## 7. 连接 Controller

1. 打开 `BP_MeleeAIController` 的 Class Defaults。
2. 将 `Behavior Tree Asset` 设置为 `BT_MeleeEnemy`。
3. 打开 `BP_EnemyCharacter`：
   - AI Controller Class = `BP_MeleeAIController`
   - Auto Possess AI = `Placed in World or Spawned`
4. 确认关卡存在覆盖战斗区的 `NavMeshBoundsVolume`，按 `P` 能看到绿色导航区域。

未设置 `Behavior Tree Asset` 时，控制器会自动继续运行原来的 Tick 追击逻辑，方便逐步切换，不会让现有敌人立即失效。

## 8. 多敌人实际规则

- 只有 1～2 名进入协调系统时，同时最多 1 名攻击。
- 3 名以上时，同时最多 2 名攻击。
- 两次新攻击授权至少间隔 0.45 秒。
- 敌人围绕玩家使用 8 个槽位，半径交替为 220 与 300。
- 槽位缓慢旋转，使等待敌人产生侧移。
- AI 丢失目标、死亡或被 UnPossess 时会释放攻击令牌与槽位。

## 9. 首轮测试

依次测试：

1. 单个敌人：发现、追击、攻击、后撤、丢失目标后搜索。
2. 两个敌人：确认不会同时起手。
3. 四个敌人：确认最多两个攻击，其余敌人在外围移动。
4. 攻击动画被受击 Montage 打断：确认其他敌人之后仍能取得令牌。
5. 敌人死亡：确认不会永久占用槽位。
