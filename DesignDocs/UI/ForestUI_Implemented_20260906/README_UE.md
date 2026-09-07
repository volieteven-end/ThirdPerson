# 森林风 UI：已实施版本

## 直接查看
正式项目 `E:/UNREAL/ue projects/ThirdPerson/ThirdPerson.uproject` 已写回并完成 C++ 编译。
重新打开这个项目，在内容浏览器打开：

- `/Game/Third/Widget/WBP_Inventory`：HUD、背包、升级选择与结算层。
- `/Game/Third/Widget/WBP_InventorySlot`：单个背包格子。
- `/Game/Third/UIArt/ForestUI`：本次新建的纹理、UI 材质和材质实例。

这些资源已经导入并引用，无须再截图、分离图集或手动安装 ZIP。

## 界面布局
- 左上：生命、精力、经验与等级；右上：关卡目标和击杀数。
- 背包：居中 5 列 × 4 行，右侧显示选中物品、数量、使用和丢弃。
- 空格子保持空白；选中状态使用金色描边，不再显示整块黄色。
- 背包和升级选择使用独立遮罩层，默认 Collapsed；仅在原有打开事件到来时显示。
- 升级使用三张横排卡片；选择后关闭面板。
- 保留原有 C++ 绑定名称与使用、丢弃、关闭、重开按钮的业务事件。

## 已修复的数据展示
1. `HPbar.Percent` 根据生命变化更新为 `Clamp(CurrentHealth / MaxHealth, 0, 1)`；最大生命为 0 时填充为 0。
2. `StaminaBar.Percent` 限制在 0～1，常态不再被旧绿色 Tint 二次染色。
3. 使用或移除物品后，选中区域的数量与图标同步刷新；空格子清理旧图标。
4. 第三张升级卡片原本反接的标题/描述引脚已纠正。
5. 新布局在选完升级后显式折叠 `LevelUpPanel`，补齐旧蓝图未实现隐藏事件的情况。

## 后续调整的位置
- 血条颜色：`MI_ForestHealth`；精力颜色：`MI_ForestStamina`；经验颜色：`MI_ForestExperience`。
- 可调整材质实例继承的颜色参数。`M_ForestFill` 负责条内渐变，`M_ForestOutline` 只画描边。
- 面板和格子分别使用 `T_ForestPanel`、`T_ForestSlot`；九宫格 Brush 保持边角，不需要重新切图。
- 在 Widget Designer 中调整相关 Size Box、Padding 和锚点，而不是通过 Render Transform 强行拉伸整体。
- 保持 `InventoryPanel` / `LevelUpPanel` 默认 Collapsed；临时预览后再改回默认状态。
- 保留现有命名控件，特别是 `HPbar`、`StaminaBar`、`ExperienceProgress`、`InventoryGrid` 和各按钮。

## 已完成验证
C++ 正式构建成功；两个 Widget 重新加载为 Up To Date。
UE 预览世界测试覆盖：20 格创建、HP 比例、药水使用与数量减少、空格子、背包开关、8 个按钮绑定。
启用 Slate 的测试还覆盖了 Lv.1 升到 Lv.5 后选择升级并关闭面板。
1920×1080 与 1280×720 的真实 UMG 离屏截图已检查。截图的 HP、SP、经验和物品是测试输入，不是修改了存档或玩法数值。
本次证据是引擎构建、资源重载、预览世界交互测试和 UMG 渲染；进游戏后可用原来的背包按键继续实机体验。

## 文件与回退
- `MODIFIED_FILE.zip`：13 个已部署文件，保留项目内相对路径。
- `DIFF_FILE.patch`：最终 C++ 差异及 Widget 的文本导出差异；二进制 Widget 使用 ZIP 内文件。
- `manifest.json`：原始/修改 SHA256；`backup` 保存原始的 2 个 Widget 与 2 个 C++ 文件。
- `VERIFICATION.txt`：命令、输入、输出、退出状态和回退验证记录。
- `tools`：生成/测试工具存档。临时编辑器辅助插件只在验证副本内，正式项目不依赖它。

如要撤销本次改动，先保存并关闭目标项目，然后在 PowerShell 执行：

```powershell
& 'E:/UNREAL/ue projects/ThirdPerson/DesignDocs/UI/ForestUI_Implemented_20260906/ROLLBACK.ps1' -Project 'E:/UNREAL/ue projects/ThirdPerson'
```

该脚本校验文件哈希后恢复原始 4 个文件、移除本次新增 9 个文件并重新编译。
若文件在本次安装后又被修改，脚本会停止以保留后续修改。`ROLLBACK.sh` 是同一流程的 Git Bash 入口。
已在另一份项目副本实际执行回退；正式项目保留新版 UI。
