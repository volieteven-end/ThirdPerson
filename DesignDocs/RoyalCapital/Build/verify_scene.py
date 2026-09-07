import unreal,pathlib,json,hashlib
root=pathlib.Path(__file__).parent
base=json.loads((root/'baseline.json').read_text())
stage=JOB.get('stage','modified')
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
instances=sum(c.get_instance_count() for a in actors for c in a.get_components_by_class(unreal.HierarchicalInstancedStaticMeshComponent))
markers=[a for a in actors if any(str(t).startswith('RC_MARKER_') for t in a.tags)]
original_ok=hashlib.sha256(pathlib.Path(base['original_map']).read_bytes()).hexdigest()==base['original_sha256']
assert original_ok,'Original map changed'
if stage=='baseline':
    assert instances==0 and len(markers)==0
    result='BASELINE PASS | instances=0 | markers=0 | original_map=unchanged'
else:
    assert instances>1000
    tags=[str(t) for a in markers for t in a.tags]
    assert sum(t=='RC_MARKER_TREASURE' for t in tags)==4
    assert sum(t=='RC_MARKER_ENEMY' for t in tags)==6
    assert sum(t=='RC_MARKER_BOSS' for t in tags)==1
    result=f'MODIFIED PASS | instances={instances} | treasure=4 | enemy=6 | boss=1 | original_map=unchanged'
report={'stage':stage,'world':world.get_path_name(),'actors':len(actors),'instances':instances,'markers':len(markers),'original_unchanged':original_ok,'literal_output':result}
(root/(stage+'_test.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
print(result)
