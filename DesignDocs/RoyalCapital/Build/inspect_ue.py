import json, pathlib, unreal
root = pathlib.Path(__file__).parent
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
reg = unreal.AssetRegistryHelpers.get_asset_registry()
meshes = []
for path in ['/Game/green_Island/meshes','/Game/LevelPrototyping/Meshes','/Game/Fab']:
    for data in reg.get_assets_by_path(path, recursive=True):
        if str(data.asset_class_path.asset_name) == 'StaticMesh':
            obj = data.get_asset()
            box = obj.get_bounding_box()
            meshes.append({'path':obj.get_path_name(),'min':[box.min.x,box.min.y,box.min.z],'max':[box.max.x,box.max.y,box.max.z],'materials':[str(m.material_interface.get_path_name()) if m.material_interface else None for m in obj.static_materials]})
out = {'world':world.get_path_name(),'actor_count':len(actors),'dirty_maps':[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()], 'dirty_content':[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()], 'meshes':meshes}
for name in ['EditorLevelLibrary','EditorLevelSubsystem','EditorActorSubsystem','StaticMeshEditorSubsystem','GeometryScriptLibrary_MeshPrimitiveFunctions','GeometryScriptLibrary_StaticMeshFunctions','GeometryScriptLibrary_MeshBooleanFunctions','DynamicMesh','AutomationLibrary','EditorLoadingAndSavingUtils','NavigationSystemV1']:
    cls=getattr(unreal,name,None)
    out[name]=[s for s in dir(cls) if not s.startswith('_')] if cls else None
(root/'ue_inventory.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
print(json.dumps({'world':out['world'],'actor_count':len(actors),'dirty_maps':out['dirty_maps'],'dirty_content':out['dirty_content'],'mesh_count':len(meshes)},indent=2))
