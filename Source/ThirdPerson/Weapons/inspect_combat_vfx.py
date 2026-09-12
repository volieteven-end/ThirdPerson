"""Read-only UE commandlet audit of the external assets needed by the combat VFX copies."""
import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()
assets = registry.get_assets_by_path('/Game/Third/Effects/Combat', recursive=True)
pending = [str(asset.package_name) for asset in assets]
seen = set()
options = unreal.AssetRegistryDependencyOptions(
    include_hard_package_references=True,
    include_soft_package_references=True,
    include_searchable_names=False,
    include_soft_management_references=False,
    include_hard_management_references=False)
while pending:
    package = pending.pop()
    if package in seen or not package.startswith('/Game/'):
        continue
    seen.add(package)
    pending.extend(str(name) for name in registry.get_dependencies(package, options))
for package in sorted(seen):
    if package.startswith('/Game/SlashTrailElemental/'):
        print('VFX_DEP ' + package)

print(unreal.CombatVFXAssetTools.inspect_sources())
