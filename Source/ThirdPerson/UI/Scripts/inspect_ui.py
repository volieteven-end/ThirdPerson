# 只读检查菜单和背包 UI 结构，用于核对蓝图控件及绑定。
import unreal
if not unreal.CombatUIAssetTools.inspect_ui():
    raise RuntimeError("UI inspection failed")
