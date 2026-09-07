import unreal,json,pathlib,collections
r=pathlib.Path(__file__).parent
data=json.loads((r/'layout.json').read_text())
groups=collections.defaultdict(list)
for s in data['items']: groups[(s['district'],s['mesh'],s['material'],s['collision'])].append(s)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
made=[a for a in actors if 'RC_HISM' in [str(t) for t in a.tags]]
for a in made[-8:]:
    c=a.get_component_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
    index=int(a.get_actor_label().split('_')[1])
    key,specs=list(groups.items())[index]
    print(json.dumps({'actor':a.get_actor_label(),'count':c.get_instance_count(),'expected':len(specs),'key':key,'first_spec':specs[0]},indent=2))
print('VOLUME_OPTIONS '+str(unreal.GeometryScriptCreateNewVolumeFromMeshOptions.__doc__))
