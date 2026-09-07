import json
from pathlib import Path
import unreal
root='/Game/Third/Bosses/Countess'
registry=unreal.AssetRegistryHelpers.get_asset_registry()
registry.scan_paths_synchronous([root], True)
assets=registry.get_assets_by_path(root, recursive=True)
rows=[]
for data in assets:
    package=str(data.package_name)
    obj=data.get_asset()
    assert obj, package
    if isinstance(obj, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(obj)
    rows.append({'package':package, 'type':str(data.asset_class_path.package_name)+'.'+str(data.asset_class_path.asset_name)})
options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True, include_searchable_names=False, include_soft_management_references=False, include_hard_management_references=False)
visited=set()
queue=[row['package'] for row in rows]
while queue:
    package=queue.pop()
    if package in visited:
        continue
    visited.add(package)
    for dep in registry.get_dependencies(package, options):
        name=str(dep)
        if name.startswith('/Game/') and name not in visited:
            queue.append(name)
forbidden=sorted(p for p in visited if 'countessplayercharacter' in p.lower())
bt=unreal.load_asset(root+'/AI/BT_CountessBoss')
blackboard=bt.get_editor_property('blackboard_asset')
assert blackboard is not None, 'Saved BT has no blackboard'
report={'asset_count':len(rows),'assets':sorted(rows,key=lambda x:x['package']),'game_dependency_count':len(visited),'forbidden_player_dependencies':forbidden,'blackboard':blackboard.get_path_name()}
path=Path(unreal.Paths.project_saved_dir())/'CountessBossDesignAudit'/'final_asset_audit.json'
path.write_text(json.dumps(report,ensure_ascii=False,indent=2), encoding='utf-8')
assert not forbidden, forbidden
assert len(rows)>=19, len(rows)
