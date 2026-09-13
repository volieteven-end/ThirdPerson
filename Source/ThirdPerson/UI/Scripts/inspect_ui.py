import unreal
if not unreal.CombatUIAssetTools.inspect_ui():
    raise RuntimeError("UI inspection failed")
