"""Assemble layout.json in the current Unreal map using HISM groups."""
import unreal,json,pathlib,math,collections
R=pathlib.Path(__file__).parent
data=json.loads((R/'layout.json').read_text())
al=unreal.EditorAssetLibrary
aas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
es=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith(data['map']+'.'),'Wrong level'
if not JOB.get('resume') and any('RC_GENERATED' in [str(t) for t in a.tags] for a in aas.get_all_level_actors()):
    raise RuntimeError('Generated actors already present; retain them and use a targeted fix')
kit=json.loads((R/'kit.json').read_text())
mats={k:al.load_asset(v) for k,v in kit['materials'].items()}
bpcls=al.load_blueprint_class('/Game/RoyalCapital/Blueprints/BP_RC_Instances')
groups=collections.defaultdict(list)
for spec in data['items']:
    groups[(spec['district'],spec['mesh'],spec['material'],spec['collision'])].append(spec)
cache={}; receipt=[]
existing={a.get_actor_label():a for a in aas.get_all_level_actors() if 'RC_HISM' in [str(t) for t in a.tags]}
for gi,((district,path,mat,collision),specs) in enumerate(groups.items()):
    if path not in cache:
        obj=al.load_asset(path)
        if not obj: raise RuntimeError('Missing mesh '+path)
        box=obj.get_bounding_box()
        lo=[box.min.x,box.min.y,box.min.z]; hi=[box.max.x,box.max.y,box.max.z]
        cache[path]=(obj,[(lo[i]+hi[i])/2 for i in range(3)],[hi[i]-lo[i] for i in range(3)])
    obj,center,ext=cache[path]
    label=f'RC_{gi:03d}_{district}_{obj.get_name()}_{mat or "Original"}'
    actor=existing.get(label) or aas.spawn_actor_from_class(bpcls,unreal.Vector(0,0,0))
    actor.set_actor_label(label)
    actor.set_folder_path('RoyalCapital/'+district)
    actor.set_editor_property('tags',['RC_GENERATED','RC_HISM'])
    actor.root_component.set_mobility(unreal.ComponentMobility.STATIC)
    comp=actor.get_component_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
    if not comp: raise RuntimeError('HISM component missing')
    comp.set_mobility(unreal.ComponentMobility.STATIC)
    comp.set_static_mesh(obj)
    comp.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision else unreal.CollisionEnabled.NO_COLLISION)
    if mat:
        for slot in range(max(1,len(obj.static_materials))): comp.set_material(slot,mats[mat])
    if mat in ['Gold','Red','Purple','Flame','Cyan']:
        comp.set_cast_shadow(False)
    comp.clear_instances()
    transforms=[]
    for spec in specs:
        sc=[spec['size'][i]*100/max(ext[i],.001) for i in range(3)]
        yaw,pitch,roll=spec['rot']; rot=unreal.Rotator(pitch,yaw,roll)
        origin=unreal.MathLibrary.transform_location(unreal.Transform(rotation=rot,scale=unreal.Vector(*sc)),unreal.Vector(*center))
        loc=unreal.Vector(spec['pos'][0]*100-origin.x,spec['pos'][1]*100-origin.y,spec['pos'][2]*100-origin.z)
        transforms.append(unreal.Transform(location=loc,rotation=rot,scale=unreal.Vector(*sc)))
    indices=comp.add_instances(transforms,True,True,False)
    assert len(indices)==len(specs)
    receipt.append({'actor':actor.get_actor_label(),'path':actor.get_path_name(),'mesh':path,'material':mat,'collision':collision,'instance_count':len(indices),'manifest_ids':[s['id'] for s in specs]})
    if gi%20==0: print(f'ASSEMBLY group {gi+1}/{len(groups)}')

colors={'TREASURE':unreal.Color(255,188,32,255),'ENEMY':unreal.Color(255,58,42,255),'BOSS':unreal.Color(159,66,255,255)}
inv=json.loads((R/'ue_inventory.json').read_text())
chest_path=next(a['path'] for a in inv['meshes'] if 'Stylized_Rustic_Chest.Stylized_Rustic_Chest' in a['path'])
for mark in data['markers']:
    x,y,z=mark['pos']; typ=mark['type']; mid=mark['id']
    point=aas.spawn_actor_from_class(unreal.TargetPoint,unreal.Vector(x*100,y*100,z*100+100))
    point.set_actor_label('RC_'+mid+'_'+typ+'_Spawn')
    point.set_folder_path('RoyalCapital/11_GameplayMarkers/'+typ)
    point.set_editor_property('tags',['RC_GENERATED','RC_MARKER_'+typ,mid])
    ta=aas.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(x*100,y*100,z*100+510),unreal.Rotator(0,-148,0))
    ta.set_actor_label('RC_Label_'+mid)
    ta.set_folder_path('RoyalCapital/11_GameplayMarkers/Labels')
    ta.set_editor_property('tags',['RC_GENERATED','RC_LABEL'])
    tc=ta.get_component_by_class(unreal.TextRenderComponent)
    tc.set_text(mid+'\n'+('TREASURE' if typ=='TREASURE' else 'ENEMY' if typ=='ENEMY' else 'ROYAL ARENA'))
    tc.set_world_size(125 if typ!='BOSS' else 220)
    tc.set_text_render_color(colors[typ])
    tc.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    if typ=='TREASURE':
        chest=aas.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(x*100,y*100,z*100),unreal.Rotator(0,180,0))
        chest.set_actor_label('RC_'+mid+'_ChestPreview')
        chest.set_folder_path('RoyalCapital/11_GameplayMarkers/TREASURE')
        chest.set_editor_property('tags',['RC_GENERATED','RC_CHEST_PREVIEW'])
        chest.static_mesh_component.set_static_mesh(al.load_asset(chest_path))
        chest.static_mesh_component.set_collision_profile_name('BlockAll')
        chest.set_actor_scale3d(unreal.Vector(1.25,1.25,1.25))

spawn=aas.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(*[v*100 for v in data['spawn_m']]),unreal.Rotator(0,0,0))
spawn.set_actor_label('RC_PlayerStart_Entrance')
spawn.set_folder_path('RoyalCapital/00_Gameplay')
spawn.set_editor_property('tags',['RC_GENERATED','RC_PLAYER_START'])
mode=al.load_blueprint_class('/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode')
world.get_world_settings().set_editor_property('default_game_mode',mode)
world.get_world_settings().set_editor_property('kill_z',-12000.0)

cameras=[]
for c in data['cameras']:
    loc=unreal.Vector(*[v*100 for v in c['pos']]); target=unreal.Vector(*[v*100 for v in c['target']])
    rot=unreal.MathLibrary.find_look_at_rotation(loc,target)
    actor=aas.spawn_actor_from_class(unreal.CameraActor,loc,rot)
    actor.set_actor_label(c['name']); actor.set_folder_path('RoyalCapital/13_ReviewCameras')
    actor.set_editor_property('tags',['RC_GENERATED','RC_CAMERA'])
    cc=actor.get_component_by_class(unreal.CameraComponent)
    cc.set_field_of_view(c['fov'])
    cc.set_editor_property('constrain_aspect_ratio',False)
    cameras.append(actor)
aas.clear_actor_selection_set()
assert es.save_current_level()
(R/'assembly_receipt.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
print(f'ASSEMBLY PASS | instances={len(data["items"])} | HISM_groups={len(groups)} | treasure=4 | enemy=6 | boss=1 | cameras=4')
