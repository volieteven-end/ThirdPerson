# Motion Warping 攻击与锁定视角方案

## 1. 当前完成的代码层

- 项目已启用 `MotionWarping` 插件并加入模块依赖。
- `ATPCCharacter` 已创建 `MotionWarpingComponent`。
- 每段近战蒙太奇成功开始时，代码都会刷新名为 `AttackTarget` 的 Warp Target。
- 未锁定时，Warp Target 朝摄像机的水平前方，默认前移 `100 cm`。
- 锁定且目标在 `600 cm` 内时，Warp Target 位于敌人前方，保留 `120 cm` 停刀距离。
- 锁定目标移动时，攻击期间会持续刷新 Warp Target。
- 尚未配置 Motion Warping Notify 的蒙太奇会使用每秒 `540°` 的平滑转向作为兼容回退，不再瞬间 `SetActorRotation`。

## 2. 必须在 Unreal 编辑器完成的蒙太奇配置

对以下主角攻击蒙太奇逐个设置：

- `AM_Sword_Attack_01`
- `AM_Sword_Attack_02`
- `AM_Sword_Attack_03`
- `AM_Sword_Attack_04`
- `AM_Sword_AirAttack`（如果空中攻击也要调整水平朝向）
- 升龙攻击当前实际使用的 Montage

### 2.1 Sequence 设置

1. 打开 Montage 中使用的原始 Animation Sequence。
2. 在 Asset Details 搜索 `Root Motion`。
3. 勾选 `Enable Root Motion`。
4. `Root Motion Root Lock` 优先使用 `Anim First Frame`；如果脚下偏移再测试 `Zero`。
5. 打开 `ABP_TPCCharacter`，将 `Root Motion Mode` 设置为 `Root Motion from Montages Only`。

### 2.2 Montage 设置

1. 打开攻击 Montage。
2. 在 `Notifies` 轨道右键。
3. 选择 `Add Notify State -> Motion Warping`。
4. 把窗口起点放在角色开始踏步/转腰的位置。
5. 把窗口终点放在剑即将命中前；不要覆盖完整收招阶段。
6. 选中 Notify State，在 Details 设置：

| 设置 | 建议值 |
|---|---|
| Root Motion Modifier | `Skew Warp` |
| Warp Target Name | `AttackTarget` |
| Warp Translation | 开启 |
| Ignore ZAxis | 开启 |
| Warp Rotation | 开启 |
| Rotation Type | `Facing` |
| Warp Rotation Method | 优先 `Slerp` |
| Warp Rotation Time Multiplier | `1.0`，转得仍快时改为 `1.25~1.5` |

窗口不要从第 0 帧直接铺到动画结束。推荐让最前面约 `0.05~0.1s` 保留原始起手姿势，再用约 `0.15~0.3s` 完成对准。

## 3. 锁定视角为什么会怪

当前相机直接从 `FollowCamera` 看向敌人的 `LockOnAimPoint`。当敌人很高、很近或处于坡面时，目标点的高度差会直接变成明显俯仰角，因此容易出现仰视、低头、角色被挤到屏幕边缘以及近距离旋转过快。

## 4. 推荐方案：双目标构图相机

不要让相机永远直接瞄准敌人的光点，而是构造一个“构图焦点”：

```text
PlayerFocus = 主角位置 + (0, 0, 80)
EnemyFocus  = 敌人 LockOnAimPoint + (0, 0, -20)
DistanceAlpha = Clamp(Player到Enemy距离 / 1200, 0, 1)
TargetWeight = Lerp(0.35, 0.55, DistanceAlpha)
CompositionFocus = Lerp(PlayerFocus, EnemyFocus, TargetWeight)
```

相机看向 `CompositionFocus`，而不是直接看向 `EnemyFocus`。这样主角始终保留在画面下方，敌人不会强行占据正中央。

### 4.1 建议参数

| 参数 | 建议起始值 |
|---|---:|
| 锁定旋转插值速度 | `5~7` |
| 单帧最大 Yaw 速度 | `180~240°/s` |
| Pitch 最小值 | `-25°` |
| Pitch 最大值 | `15°` |
| 敌人焦点向下偏移 | `20~35 cm` |
| 近距离相机臂长 | `330~380 cm` |
| 中距离相机臂长 | `420~500 cm` |
| 目标切换镜头混合 | `0.15~0.25s` |

### 4.2 必须加入的稳定措施

1. **Yaw 与 Pitch 分开计算**：Yaw 跟随敌人，Pitch 跟随双目标构图焦点。
2. **角速度上限**：使用 `RInterpConstantTo` 或自定义最大角速度，避免敌人穿过身边时镜头瞬转。
3. **近距离降权**：敌人越近，构图越偏向主角，避免仰视敌人头顶。
4. **死区**：目标在屏幕中心一定范围内时不微调相机，减少抖动。
5. **遮挡处理**：敌人被墙遮挡持续约 `0.6~1.0s` 后才解除锁定，避免一帧遮挡就跳镜头。
6. **目标切换缓冲**：切换敌人时先保存旧焦点，用 `0.2s` 插值到新焦点。
7. **相机与角色转向解耦**：相机负责构图，Motion Warping 负责攻击对准，二者不要同时瞬间修改角色 Rotation。

## 5. 实施顺序

1. 先给 `AM_Sword_Attack_01` 添加 Motion Warping Notify，只测试这一段。
2. 确认未锁定攻击朝视角前方且没有瞬转。
3. 锁定敌人，在 `150 / 300 / 550 cm` 三种距离测试吸附。
4. 调整 `AttackMagnetismStopDistance`，确保剑能命中但胶囊不会贴住敌人。
5. 将相同 Notify 配置复制到其他攻击 Montage，并分别校正窗口终点。
6. 最后再实现双目标构图相机，避免同时排查攻击和镜头两个系统。
