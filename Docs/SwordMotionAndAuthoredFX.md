# UE5 武器换手与动画特效

## 本次修改流程

1. 对照 SwordAnimsetPro 的 UE4 原动画。原骨架的 `Weapon_R` 是带动画轨道的骨骼，不是固定插槽；UE5 重定向结果缺少这条轨道，因此剑一直锁在右手。
2. 在 Manny/Quinn Simple 网格及共享骨架中补入 `hand_r → sword_motion` 控制骨，将其平移重定向模式设为 Animation。根据原动画武器相对左右握点的变换，适配 UE5 的手部位置，连续烘焙换手和翻剑运动。
3. 为地面连击 01～04 的预览序列及游戏用 `AS_` 序列增加 `sword_motion` 轨道，共 8 个序列。没有重建蒙太奇，也没有修改身体轨道、根运动、时长、伤害通知或取消窗口。
4. `weapon_r` 插槽改为挂在控制骨上。`SwordTrailBase/Tip` 和原资源的 `Weapon_R_Trail_A/B` 也跟随同一控制骨，确保实际武器、动画拖尾及既有武器采样使用同一位置来源。
5. 默认关闭 `WeaponVFXComponent.bEnableAutomaticEffects`，不再创建 C++ 自动 Niagara 刀光及 Buff 剑身附魔，也不再屏蔽动画自带的 `P_Trail`。伤害窗口不再自动生成刀光；已有动画 Trail/Niagara 通知保留。未改动 `NS_TEST`、Boss 特效或命中反馈。
6. 装备武器时关闭攻击采样调试绘制，隐藏武器上的 Shape 辅助组件；不调用关闭碰撞的接口。伤害数值和检测半径未改动，检测位置跟随修复后的剑。

## 在 Unreal 中查看

- 骨架：`/Game/Characters/Mannequins/Meshes/SK_Mannequin`。
- 预览动画：`/Game/Third/SwordAnimation/Attack_Combo_01_02_Anim`。
- 游戏用动画：`/Game/Third/Actions/Sword/Sequences/AS_Attack_Combo_01_02_Anim`。
- 在骨架树展开 `hand_r → sword_motion`，可以看到 `weapon_r` 和拖尾端点插槽。
- 预览剑应附加到 **weapon_r**，不要附加到固定的 `HandGrip_R`；后者仍是右手握点，不包含换手运动。现有该剑的骨架预览附件已重定向。
- 修改刀光请编辑实际播放序列/蒙太奇中的 Trail 或 Niagara 通知，注意蒙太奇和底层序列可以各自有通知。删除某一层的通知不会删除另一层的通知。
- 剑身本身的青蓝色来自原武器材质，本次没有修改材质。

## 后续维护

编辑器工具 `SwordMotionAssetTools.BuildSwordMotion` 只针对上述 4 段连击（两套序列）追加/更新武器轨道。当前资源已经保存，无需重复运行。后续若重新重定向并覆盖这些序列，需要重新补入轨道；执行前先保存并关闭正在编辑的资源，避免未保存版本覆盖磁盘结果。

源代码：`Source/ThirdPerson/Animation/SwordMotionAssetTools.cpp`。

回归入口：

- `ThirdPerson.SwordMotion.AssetContract`：控制骨/插槽、8 个序列的原生拖尾及运行时压缩轨道。
- `ThirdPerson.VFX.PIE.LifecycleAt30_60_120`：普通/特殊攻击、第二和第三段连击、收刀、Buff、死亡重生，不分配自动刀光，保留动画拖尾，隐藏辅助形状。
- `ThirdPerson.VFX.PIE.BossAccentWindows`：Boss 表现不受主角开关影响。
