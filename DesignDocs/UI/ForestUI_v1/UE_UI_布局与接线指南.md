# 森林动作游戏 UI 方案 · 墨绿与旧金
日期：2026-09-06

## 1. 本次方案与当前界面的问题

本次交付两张界面效果图和这份 UE 配置指南；没有修改 C++、配置文件或 .uasset。
效果图由内置 image_gen 生成，画面中的血量、击杀数和道具是布局示例，不是游戏运行数据。
两张 PNG 实际尺寸均为 1672 × 941；以下 UMG 参数按 1920 × 1080、DPI Scale = 1 的逻辑尺寸设计，作为落地起点，不要求对生成图逐像素照搬。

- 战斗 HUD：[01_combat_hud.png](E:/UNREAL/ue%20projects/ThirdPerson/DesignDocs/UI/ForestUI_v1/01_combat_hud.png)
- 背包界面：[02_inventory.png](E:/UNREAL/ue%20projects/ThirdPerson/DesignDocs/UI/ForestUI_v1/02_inventory.png)
- 可复用生成提示词：[prompts.txt](E:/UNREAL/ue%20projects/ThirdPerson/DesignDocs/UI/ForestUI_v1/prompts.txt)

截图里主要有四个问题：
1. 血条与大段白色数值的视觉权重相近，左上角显得过重。
2. 任务、击杀分别自由定位，右上角文字相互覆盖。
3. 背包格子采用默认按钮外观，亮黄色整块选中态盖住了物品信息。
4. HUD、背包、升级、结算混在一个画布中，编辑时难以区分状态。

**Designer 中几个弹窗重叠，并不直接说明游戏运行时也会同时显示。** 应分别预览各状态；运行时管理 Visibility，而不是把所有弹窗移到屏幕不同角落。

设计原则：常驻信息靠边、中央留给战斗；背包打开时中央用于操作；旧金表示交互和选中，红/绿/蓝分别表示生命/精力/经验。第一版不增加地图、技能栏、货币或装备系统。

## 2. 视觉规范与位置

### 颜色与字体

| 用途 | 建议值 |
|---|---|
| 深墨绿面板 | #14251F，背包主体不透明度约 0.94 |
| 格子底色 | #192A23 |
| 普通边线 | #655C40，约 1 px |
| 主要装饰/描边 | #B99B61 |
| 选中描边 | #E5C56C，约 2 px，不铺满亮黄色 |
| 正文 | #EEE9DA |
| 次要文字 | #AAAFA2 |
| 生命 / 精力 / 经验 | #C63732 / #4C9F59 / #38B8C2 |
| 全屏遮罩 | 黑色 Alpha 约 0.45；首版用 Image/Border 即可 |

标题可用有中文字形的衬线字体；HUD 数字和正文优先使用清楚的无衬线字体。建议标题 32，HUD 数字/正文 20–22，按钮 22，辅助文字 16–18；这些是 1080p、DPI=1 下的起始值。

### Canvas 参数

仅在最外层 Canvas Slot 设置锚点；内部由 Vertical Box / Horizontal Box / Overlay 排版。

| 区域 | Anchors Min=Max | Alignment | Position | 建议尺寸 |
|---|---|---|---|---|
| 左上 StatusPanel | (0, 0) | (0, 0) | (32, 32) | 480 × 160 |
| 右上 ObjectivePanel | (1, 0) | (1, 0) | (-32, 32) | 320 × 112；长任务允许增高 |
| 底部 InteractionPanel | (0.5, 1) | (0.5, 1) | (0, -40) | 约 240 × 48 |
| 中央 InventoryPanel | (0.5, 0.5) | (0.5, 0.5) | (0, 0) | 1088 × 760 |
| ModalScrim | Min(0,0), Max(1,1) | 默认 | 四边 Offset=0 | 全屏拉伸 |

右上把任务和击杀放进**同一个 Vertical Box**，各占一行，统一右对齐。任务正文启用自动换行；不要把两个 TextBlock 各自拖到同一坐标。

Anchors 的位置随父 Canvas 尺寸计算；它解决边缘定位，DPI 解决整体缩放，两者不是同一件事。[Epic：Anchors](https://dev.epicgames.com/documentation/unreal-engine/umg-anchors-in-unreal-engine-ui?lang=en-US)

分辨率适配：先保留项目已有 DPI 规则，确认 1080p 设计基准；若采用 Shortest Side，可从 720→0.667、1080→1、1440→1.333 的曲线起步，再实测。宽屏靠左右锚点展开，不把整张 HUD 拉伸成背景图片。通过 Project Settings → User Interface 检查 DPI Scale Rule / DPI Curve。[Epic：DPI Scaling](https://dev.epicgames.com/documentation/en-us/unreal-engine/dpi-scaling-in-unreal-engine)

## 3. 第一版 UMG 层级：保留当前父类和控件绑定

先在当前 WBP_Inventory 内整理容器，不急于拆成多个 Widget Blueprint。

```text
WBP_Inventory（父类仍为 InventoryWidget）
└─ RootCanvas
   ├─ HUDLayer                         ZOrder 0，全屏拉伸
   │  ├─ StatusPanel
   │  │  └─ VerticalBox
   │  │     ├─ HPRow：图标 + HPbar + HealthTextBlock
   │  │     ├─ SPRow：图标 + StaminaBar + StaminaTextBlock
   │  │     └─ XPRow：等级/经验文字 + ExperienceProgress
   │  ├─ ObjectivePanel
   │  │  └─ VerticalBox：ObjectTextBlock、KillTextBlock
   │  └─ InteractionPanel：InteractionTextBlock
   ├─ ModalScrim                       ZOrder 10，默认 Collapsed
   ├─ InventoryPanel                   ZOrder 20，默认 Collapsed
   │  └─ SizeBox → Border → VerticalBox
   │     ├─ Header：背包、20 格、关闭按钮
   │     ├─ Body：HorizontalBox
   │     │  ├─ InventoryGrid           UniformGridPanel，保留原名
   │     │  ├─ 间隔与竖向分割线
   │     │  └─ ItemDetailPanel
   │     │     ├─ SelectedItemImage    可选，见第 6 节
   │     │     ├─ 名称、数量、提示
   │     │     └─ 使用按钮、丢弃按钮
   │     └─ Footer：帮助文字、Esc 关闭提示
   ├─ LevelUpPanel                     ZOrder 30，默认 Collapsed
   └─ ResultPanel                      ZOrder 40，默认 Collapsed
```

以上部分容器名称是建议新名；HPbar、StaminaBar、ExperienceProgress 等已有控件应保留，避免破坏你现有蓝图图表引用。

**InventoryGrid 是 C++ 的 BindWidget，必须继续叫 InventoryGrid，并保持 UniformGridPanel 类型，留在当前 WBP_Inventory 的控件树中。**
把它直接搬到一个独立子 Widget 后，父类的 BindWidget 不会自动跨子 Widget 找到它。等界面稳定，再通过明确的数据转发拆分。

装饰图层设为 Not Hit-Testable；含按钮的父容器可用 Not Hit-Testable (Self Only)，保留子按钮交互。不要把整个背包设成 Self & All Children，否则子按钮也失去鼠标命中。

隐藏弹窗时使用 Collapsed。设计器中一次只预览一个弹窗；不要仅用 Render Opacity=0 假装关闭，因为控件仍可能占位或拦截输入。

## 4. 血条、精力条、经验条怎么搭

### 排版，不靠 Render Scale 拉长

每一行使用 Horizontal Box：
- 图标用 SizeBox 限定为 24 × 24。
- 进度条外用 SizeBox 限宽约 280–288。
- 生命填充区域高度 22，精力 16，经验 8；外边框可略大。
- 数值区域固定宽度约 104，右对齐；行内间隔 8。
- 外层容器给足宽度；不要让图标使用原始贴图的几百像素尺寸挤压条形区域。

一根条内部可用：

```text
SizeBox（规定整个条的尺寸）
└─ Overlay
   ├─ 底槽 Image
   ├─ ProgressBar（Fill Image 仅放实心填充纹理）
   └─ 外框 Image（只负责装饰，不随 Percent 缩短）
```

ProgressBar 的 Overlay Slot 水平和垂直均 Fill。检查 Border Padding 没有把内部高度吃光。Fill Image 使用紧贴实际填充内容裁切的纹理，不用带大量透明留白的整张条形图集。

Percent 取 0～1；方向 Left to Right。有纹理的填充可试 Mask，减少缩短时把纹理一起压扁的观感。Fill Color and Opacity 先设白色，避免给已着色的贴图再乘一次颜色。外框与填充是两层，不把整根外框塞进 Fill Image。[Epic：SProgressBar](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Slate/SProgressBar)

### 已核对的项目数据入口

| 数据 | 当前 C++ 提供的 BP 事件 | 界面连接 |
|---|---|---|
| 生命文本 | RefreshHealthText(InText) | HealthTextBlock.SetText |
| 精力文本 | RefreshStaminaText(InText) | StaminaTextBlock.SetText |
| 精力比例 | RefreshStaminaPercent(Percent) | StaminaBar.SetPercent |
| 低精力样式 | RefreshStaminaBarStyle(Percent, bIsLowStamina) | 低于阈值时改为警示颜色；减少持续闪烁 |
| 等级/经验 | RefreshLevelProgress(LevelText, ExperiencePercent) | LevelTextBlock.SetText、ExperienceProgress.SetPercent |
| 击杀 | RefreshKillText(InText) | KillTextBlock.SetText |
| 目标 | RefreshObjectiveText(InText) | ObjectTextBlock.SetText |
| 拾取提示 | RefreshInteractionPrompt(PromptText, bVisible) | 设置文本，并切换 InteractionPanel 的可见性 |

**值得补查的一点：当前 HandleHealthChanged 只转发生命文本，没有与精力对称的生命 Percent 事件。** 不代表你的 HPbar 一定没更新——你可能已在蓝图里接了别的绑定；这次没有读取 .uasset 内部图表。

若 HPbar 当前未更新，可在 RefreshHealthText 事件中：
1. 取得 Get Owning Player Pawn。
2. Get Component by Class，取得 HealthComponent，并验证对象有效。
3. 读取已暴露为 BlueprintReadOnly 的 CurrentHealth / MaxHealth。
4. MaxHealth > 0 时计算 Clamp(CurrentHealth / MaxHealth, 0, 1)，否则用 0。
5. 调用 HPbar.SetPercent。初始化和后续生命事件都会走这条更新路径；不必在 Tick 中反复查询。

如果已有等价绑定，只保留一条更新路径即可。

图中的“100 / 100”和独立 Lv.1 是精简后的目标样式。当前 C++ 传入的文本仍带 HP:、SP:、Lv./XP 等组合前缀。第一版可先保留原文本；后续把原始数值传给 BP，再用 Format Text 统一排版和本地化，不要拆字符串来倒推数值。当前 LevelComponent 的几个 GetLevel/GetExperience 方法并未标为 BlueprintPure，不能假定它们已能在蓝图里直接调用。

## 5. 背包格子：改 WBP_InventorySlot，不手动摆 20 个按钮

已核对：
- InventoryComponent 的 C++ 默认 MaxSlots = 20。
- UInventoryWidget 的默认 InventoryGridColumns = 5。
- RefreshInventoryGrid 根据容量创建格子，行 = Index / Columns，列 = Index % Columns。
- 蓝图 Class Defaults 的覆盖值仍以你编辑器中实际值为准。
- 内容目录存在 WBP_InventorySlot；父 Widget 的 InventorySlotWidgetClass 应指向它。

推荐每个格子：

```text
WBP_InventorySlot（父类 InventorySlotWidget）
└─ SizeBox（96 × 96）
   └─ Button（内容 Padding=0）
      └─ Overlay
         ├─ SlotBackground
         ├─ ItemImage（约 68 × 68，居中，保持宽高比）
         ├─ CountText（右下角，Padding=6）
         └─ SelectionFrame（不拦截鼠标）
```

InventoryGrid 的 Slot Padding 四边各 8，因此每个格子的布局单元约为 112 × 112，5×4 区域约 560 × 448。外面不要再让水平 Fill 拉长格子；保留正方形。

| 状态 | 表现 |
|---|---|
| Normal | 深绿底，细暗金边 |
| Hovered | 底色略亮，边线稍亮 |
| Selected | 2 px 金色外框，轻微内发光；道具图标不变黄 |
| Empty | 清掉图标与数量，保留安静的底槽 |
| 按钮不可用 | 保留文字可读性，降低对比度，不用亮黄色提示 |

与现有代码接线：
- Button.OnClicked → NotifyClicked。
- Button.OnHovered → SetHovered(true)。
- Button.OnUnhovered → SetHovered(false)。
- RefreshSlot(ItemName, ItemCount, InIcon) → ItemImage.SetBrushFromTexture；设置数量。
- RefreshEmptySlot → 清空图标、隐藏数量；不要遗留上一个物品图片。
- RefreshSlotStyle(bSelected, bHovered) → 优先显示 Selected，其次 Hovered，再 Normal。
- 使用按钮 → UseSelectedItem。
- 丢弃按钮 → DropSelectedItem。
- 关闭按钮 → CloseInventory。

选中信息使用 RefreshSelectedItemInfo：
- bHasItem=false：显示“选择物品以查看详情”，禁用使用/丢弃。
- bHasItem=true：显示名称和数量；丢弃可用。
- bCanUse 控制使用按钮。当前原生使用函数仍会检查满血等条件；提示接 ShowInventoryHint，在背包内部显示，不遮住屏幕中央。

旧 RefreshInventoryText 产生的整段文本列表，在采用格子展示后可以将对应文本控件 Collapsed，避免列表与格子重复显示。

## 6. 效果图里哪些能直接做，哪些需要加数据

| 设计内容 | 实现情况 |
|---|---|
| 边缘 HUD、任务分行、统一字号、背包分区 | 调 UMG 布局即可 |
| 20 个格子、选中、使用、丢弃、关闭 | 已有 C++ 逻辑和 BP 事件，保留接线 |
| 单个格子的道具图标 | 已从 ItemDefinition.Icon 传入 RefreshSlot |
| 右侧选中道具的大图 | 当前 RefreshSelectedItemInfo 没有 Icon 参数，建议后续新增参数再更新大图 |
| 道具名称和数量 | 已有参数 |
| 道具长描述、稀有度 | 当前 ItemDefinition 没有这些字段；这版不要求增加 |
| HUD 中文与拆分数字 | 当前 C++ 拼接了英文文本；视觉整理后再集中调整数据事件与 FText 本地化 |
| 背包打开时暂停战斗 | 当前 ToggleInventory 使用 GameAndUI，并没有暂停；需要统一调整控制器逻辑 |

图中的草药、水晶、袋子用于展示“有物品/无物品”状态，不表示这些道具已经配置进项目。第一版全部换成你现有 ItemDefinition 的图标即可。

Esc 关闭属于本方案建议的交互：
- 在背包可见且获得焦点时处理 Esc，调用 CloseInventory，并返回 Handled。
- 主 Widget 打开 Is Focusable；恢复游戏输入由控制器统一处理。
- 仅在背包打开时调用 CloseInventory，因为它目前内部委托 ToggleInventory，重复调用会再次打开。
- Esc 不应同时触发背包关闭和暂停菜单。

若选择“单机暂停式背包”，在控制器的开/关流程成对处理暂停、输入模式、鼠标与焦点；不要只在 UI 一侧暂停而漏掉关闭恢复。目前仅换图片与容器不会自动阻止角色移动或攻击。

## 7. 状态管理与制作顺序

建议前景弹窗互斥，优先级：结算 > 升级选择 > 背包 > 无弹窗。切换由一个明确的状态入口管理，避免多个蓝图事件各自打开一个面板。

| 状态 | HUD | 背包 | 升级 | 结算 | 全屏遮罩 |
|---|---|---|---|---|---|
| 战斗 | 显示 | 收起 | 收起 | 收起 | 收起 |
| 背包 | 显示在遮罩下 | 显示 | 收起 | 收起 | 显示 |
| 升级选择 | 显示在遮罩下 | 收起 | 显示 | 收起 | 显示 |
| 结算 | 可收起 | 收起 | 收起 | 显示 | 显示 |

现有 ShowLevelUpChoices、HideLevelUpChoices、RefreshGameResult 可作为 UI 状态事件入口；需要在这些事件中统一管理其他面板，而不是假定当前代码已完成互斥。

制作顺序：
1. 先复制 WBP_Inventory / WBP_InventorySlot 作为备份，在原 Widget 保留必要名称与父类。
2. 整理 HUD 左上/右上/底部三个区域，解决文字覆盖。
3. 将背包默认设为 Collapsed，重新组织标题、格子、详情、底部提示。
4. 统一格子 Normal/Hovered/Selected 状态，换掉整块黄色选中背景。
5. 核对 HP、SP、XP 的 Percent 更新和空格子清理。
6. 最后制作可复用装饰组件，再补右侧大图等数据接口。

### 图片怎样用于 UE

本次两个 PNG 是整体设计效果图，**不是可直接当完整 HUD 使用的组件图集**。文字、百分比、按钮和数量应由 UMG 动态渲染。

优先使用 UMG 的纯色底、描边和现有图标完成第一版。后续独立组件建议：
- 面板角饰与边框：可拉伸的边框资源，边角保持比例。
- Slot 的 Normal / Hovered / Selected 外框：同尺寸、同内边距，透明背景。
- HP / SP / XP 的紧裁切填充纹理：只含填充部分。
- 道具图标：透明背景，统一 256 × 256 或 512 × 512 画布，主体占比一致。

不建议从完整效果图截图抠按钮或血条，会把文字、光影和场景底色一起带进去。项目原有 UIArt 资源仍保留，可先复用；本次没有导入新组件纹理或替换资源。

### 落地后验收（尚待 UE 中实施和运行验证）

- [ ] 1280×720、1920×1080、2560×1440，以及 21:9 下，左右 HUD 均未出屏。
- [ ] 任务文本变长时向下换行，不盖住击杀数字。
- [ ] HP=0/50/100%、SP=0/25/100%、经验升级前后显示正确。
- [ ] 背包恰好 5×4，格子保持正方形，空格子没有旧图标。
- [ ] 点击物品更新选中框与详情；使用/丢弃后图标与数量一致。
- [ ] 打开与关闭背包、升级、结算时，没有两个主要弹窗同时显示。
- [ ] 鼠标操作 UI 不误触发攻击；关闭后 WASD、视角、攻击恢复。
- [ ] 连续多次开关背包、Esc 关闭、重新开始后，输入状态正常。
- [ ] 使用图片中展示的大图或拆分数值前，已补齐第 6 节指出的数据接口。

## 8. 本次核对的本地文件

- E:/UNREAL/ue projects/ThirdPerson/Source/ThirdPerson/UI/InventroyWidget.h
- E:/UNREAL/ue projects/ThirdPerson/Source/ThirdPerson/UI/InventroyWidget.cpp
- E:/UNREAL/ue projects/ThirdPerson/Source/ThirdPerson/UI/InventorySlotWidget.h
- E:/UNREAL/ue projects/ThirdPerson/Source/ThirdPerson/UI/InventorySlotWidget.cpp
- E:/UNREAL/ue projects/ThirdPerson/Source/ThirdPerson/Components/InventoryComponent.h
- E:/UNREAL/ue projects/ThirdPerson/Source/ThirdPerson/Components/HealthComponent.h
- E:/UNREAL/ue projects/ThirdPerson/Source/ThirdPerson/Components/LevelComponent.h
- E:/UNREAL/ue projects/ThirdPerson/Source/ThirdPerson/Items/ItemDefinition.h
- E:/UNREAL/ue projects/ThirdPerson/Source/ThirdPerson/Character/TPCPlayerController.cpp
- E:/UNREAL/ue projects/ThirdPerson/Content/Third/Widget/WBP_Inventory.uasset
- E:/UNREAL/ue projects/ThirdPerson/Content/Third/Widget/WBP_InventorySlot.uasset

C++ 已读取；蓝图依据你提供的 Designer 截图并确认资产文件存在，本次没有解包检查蓝图内部事件图。

