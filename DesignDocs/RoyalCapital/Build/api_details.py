import unreal,json,pathlib
out={}
for cname,methods in {
 'GeometryScript_Primitives':['append_box','append_cylinder','append_cone','append_torus','append_simple_extrude_polygon'],
 'GeometryScript_NewAssetUtils':['create_new_static_mesh_asset_from_mesh','create_new_volume_from_mesh'],
 'GeometryScript_MeshBooleans':['apply_mesh_boolean'],
 'SubobjectDataSubsystem':['k2_gather_subobject_data_for_blueprint','add_new_subobject'],
 'SubobjectDataBlueprintFunctionLibrary':['get_data','get_object'],
 'BlueprintEditorLibrary':['compile_blueprint'],
 'GeometryScript_MeshModeling':['apply_mesh_bevel'],
 'GeometryScript_Collision':['set_static_mesh_collision_from_mesh'],
 'MaterialEditingLibrary':['get_material_function_input_names','get_vector_parameter_names','get_scalar_parameter_names','get_texture_parameter_names'],
 'HierarchicalInstancedStaticMeshComponent':['add_instances'],
 'EditorActorSubsystem':['spawn_actor_from_class']
}.items():
    cls=getattr(unreal,cname,None)
    out[cname]={m:str(getattr(cls,m,None).__doc__) for m in methods}
for cname in ['GeometryScriptCreateNewStaticMeshAssetOptions','GeometryScriptCollisionFromMeshOptions','AddNewSubobjectParams','GeometryScriptPrimitiveOptions','MaterialExpressionCustom','CustomInput']:
    cls=getattr(unreal,cname,None)
    out[cname]=str(cls.__doc__) if cls else None
for name in ['wall_tile','wall','floor_tile','roof']:
    path='/Game/green_Island/materials/construction_props_materials/MI_'+name+'_Inst'
    mat=unreal.load_asset(path)
    if mat: out['mat_'+name]={'vectors':[str(x) for x in unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)],'scalars':[str(x) for x in unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)],'textures':[str(x) for x in unreal.MaterialEditingLibrary.get_texture_parameter_names(mat)]}
(pathlib.Path(__file__).parent/'api_details.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
print('API_DETAILS_READY')
