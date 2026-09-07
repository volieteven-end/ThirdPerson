import unreal,json,pathlib
out={}
out['geometry_names']=[x for x in dir(unreal) if 'GeometryScript' in x or x in ['DynamicMesh','GeneratedDynamicMeshActor','DynamicMeshActor','LevelEditorSubsystem']]
for name in ['Actor','HierarchicalInstancedStaticMeshComponent','EditorLevelLibrary','LevelEditorSubsystem','StaticMeshEditorSubsystem','DynamicMesh','GeometryScript_Primitives','GeometryScript_Assets','GeometryScript_MeshBooleans','GeometryScript_Collision','EditorLoadingAndSavingUtils','PostProcessSettings']:
    obj=getattr(unreal,name,None)
    if obj:
        names=[x for x in dir(obj) if not x.startswith('_')]
        out[name]={'names':names}
        selected=names if name.startswith('Geometry') else [n for n in names if any(k in n for k in ['component_by','instance_component','new_level','new_blank','save_map','save_current','screenshot','collision','game_view','pilot','viewport','game_mode'])]
        out[name]['docs']={n:str(getattr(obj,n).__doc__) for n in selected}
out['screenshot']=unreal.AutomationLibrary.take_high_res_screenshot.__doc__
out['trace']=unreal.SystemLibrary.line_trace_single.__doc__
root=pathlib.Path(__file__).parent
(root/'ue_api.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
print(json.dumps({'geometry_names':out['geometry_names'],'actor_components':{n: str(getattr(unreal.Actor,n).__doc__) for n in dir(unreal.Actor) if 'component_by' in n or 'instance_component' in n},'screenshot':out['screenshot']},indent=2))
