import json,pathlib,hashlib,shutil,unreal
root=pathlib.Path(__file__).parent
project=root.parents[2]
base='/Game/RoyalCapital'
level=base+'/Maps/L_RoyalCapital'
es=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
aas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
al=unreal.EditorAssetLibrary
if al.does_asset_exist(level):
    raise RuntimeError('New level already exists; use explicit continuation instead of replacing it')
if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
    raise RuntimeError('Current map has unsaved work; preserve it before changing levels')
original=project/'Content/ThirdPerson/Lvl_ThirdPerson.umap'
baseline={'original_map':str(original),'original_sha256':hashlib.sha256(original.read_bytes()).hexdigest(),'original_world':unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name(),'original_actor_count':len(aas.get_all_level_actors()),'untouched_dirty_content':[p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]}
for folder in ['Maps','Meshes','Materials','Blueprints']:
    al.make_directory(base+'/'+folder)
assert es.new_level(level,False)
assert es.save_current_level()
mapfile=project/'Content/RoyalCapital/Maps/L_RoyalCapital.umap'
shutil.copy2(mapfile,root/'L_RoyalCapital.BASELINE.umap')
baseline.update(new_map=str(mapfile),empty_sha256=hashlib.sha256(mapfile.read_bytes()).hexdigest(),empty_actor_count=len(aas.get_all_level_actors()))
(root/'baseline.json').write_text(json.dumps(baseline,indent=2),encoding='utf-8')

# A reusable instancing actor: one HISM component, no game logic and no ticking.
factory=unreal.BlueprintFactory()
factory.set_editor_property('parent_class',unreal.Actor)
bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_RC_Instances',base+'/Blueprints',unreal.Blueprint,factory)
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
handles=sub.k2_gather_subobject_data_for_blueprint(bp)
h,fail=sub.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=handles[0],new_class=unreal.HierarchicalInstancedStaticMeshComponent,blueprint_context=bp))
if str(fail): raise RuntimeError(str(fail))
assert unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert al.save_loaded_asset(bp)
print('SETUP PASS | new independent map created | empty baseline preserved | HISM blueprint compiled')
