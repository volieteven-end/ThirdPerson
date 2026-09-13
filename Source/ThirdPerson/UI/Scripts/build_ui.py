"""Only run after the user saves and closes Unreal Editor."""
import unreal
if not unreal.CombatUIAssetTools.build_ui_upgrade():
    raise RuntimeError("UI upgrade failed; inspect the log")
