# 编辑器写入工具：会保存菜单、背包分页及关联展示资源，不是运行时界面逻辑。
"""Only run after the user saves and closes Unreal Editor."""
import unreal
if not unreal.CombatUIAssetTools.build_ui_upgrade():
    raise RuntimeError("UI upgrade failed; inspect the log")
