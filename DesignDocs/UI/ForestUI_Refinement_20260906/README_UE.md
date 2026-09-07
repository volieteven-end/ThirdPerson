# HUD 调淡与胸口锁定光点：已实施

正式项目：`E:/UNREAL/ue projects/ThirdPerson/ThirdPerson.uproject`。7 个文件已部署，C++ 编译和蓝图重载通过。

## 这轮改动
- 左上 `ForestStatusPanel` 背景不透明度：100% → 35%。文字、图标、血条保持原不透明度。
- 右上 `ForestObjectivePanel` 背景不透明度：100% → 32%；尺寸 340×152 → 280×112。
- 右上目标字体 20 → 16，击杀数字字体 18 → 14，内部间距随面板缩小。
- 锁定标记从橙色环改成白色亮芯和柔和光晕。屏幕占位 24，实际亮芯直径约 8；轻微呼吸幅度 0.8。
- 两种敌人均挂到 `spine_03` 胸骨，偏移为 0；没有该骨骼时退到胶囊体的上半身位置。
- 取消锁定、死亡会隐藏标记；死亡后的旧显示请求也会被忽略。
- 镜头参考位置与显示光点分离，原有锁定输入、相机插值和攻击转向逻辑保持原样。

## UE 中继续微调
### HUD
打开 `/Game/Third/Widget/WBP_Inventory`：
- `ForestStatusPanel` → Brush Color 的 A：目前 0.35。
- `ForestObjectivePanel` → Brush Color 的 A：目前 0.32。
- `ForestObjectivePanel` 的 Canvas Slot Size：280×112。
- 调背景时改 Brush Color，不要调整个面板的 Render Opacity，否则文字也会一起变淡。

### 光点
打开 `/Game/Third/Character/BP_EnemyCharacter` 或 `BP_EnemyRangedCharacter` → Class Defaults → Lock On：
- `Lock On Indicator Socket Name`：spine_03。
- `Lock On Indicator Offset`：(0, 0, 0)，用于微调胸口光点。
- `Lock On Indicator Size`：24，想更小可试 20。
- `Lock On Indicator Pulse Amount`：0.8，设为 0 可取消呼吸缩放。
- `Lock On Camera Reference ...` 是独立的镜头参考配置，调整光点时保持这些参数不变。

光点由原生 UMG 绘制，不需要再绑定 Niagara 或额外光源。现有锁定操作直接使用新版标记。
保留了你原来的近战敌人镜头偏移 -70 和下移 30；远程敌人仍为偏移 28、下移 70。

## 验证与文件
已验证原生、近战、远程三类敌人的显示/隐藏、移动跟随、死亡隐藏、相机参考不受光点移动影响；新旧相机坐标一致。
已检查真实 UMG 离屏渲染，包括 1920×1080、1280×720 和深浅两种背景上的光点。
预览里的数值来自测试世界，不是存档修改。完整记录见 `VERIFICATION.txt`。

`MODIFIED_FILE.zip` 是已部署的文件副本；无需重复导入。`DIFF_FILE.patch` 包含代码与 Widget 导出差异。
本轮回退只撤销这 7 个文件，不撤销上一轮 UI 重做：保存并关闭目标项目后执行

```powershell
& 'E:/UNREAL/ue projects/ThirdPerson/DesignDocs/UI/ForestUI_Refinement_20260906/ROLLBACK.ps1' -Project 'E:/UNREAL/ue projects/ThirdPerson'
```

回退脚本会检查后续修改、恢复本轮原件并重新编译；已在独立副本实际执行验证。
