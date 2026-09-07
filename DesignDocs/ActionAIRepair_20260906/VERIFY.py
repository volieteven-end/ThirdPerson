import sys,json,hashlib,math
from pathlib import Path
D=Path(__file__).resolve().parent
R=Path('E:/UNREAL/ue projects/ThirdPerson/Saved/ActionAIRepair/20260906_v1')
phase=sys.argv[1].upper()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def auto(phase,n):
    a=read(R/(phase.lower()+'_automation/index.json'))
    assert len(a['tests'])==n and all(t['state']=='Success' for t in a['tests'])
if phase=='BASELINE':
    auto(phase,5)
    a=read(R/'baseline_action_and_nav_build.json');p=read(R/'baseline_pie.json')
    assert 'dash=0 attack=1' in a['125']['state']
    e=[x for x in p[0]['enemies'] if 'Ranged' not in x['name']]
    assert len(e)==2 and all(x['nav_path_valid']==False and 'FAILED' in x['forced_move_result'] for x in e)
    print('BASELINE: nav=FAILED; attack-dash=BLOCKED; tests=5/5; exit=0')
elif phase=='MODIFIED':
    auto(phase,8)
    a=read(R/'modified_runtime.json')
    assert a['result']=='PASS' and len(a['checks'])==13 and all(c['pass'] for c in a['checks'])
    for item in read(D/'manifest.json'):
        p=Path('E:/UNREAL/ue projects/ThirdPerson')/item['path']
        assert hashlib.sha256(p.read_bytes()).hexdigest()==item['modified_sha256']
    print('MODIFIED: nav=PASS; cancel+chain=PASS; tests=8/8; PIE=13/13; exit=0')
elif phase=='ROLLBACK':
    auto(phase,5)
    a=read(R/'rollback_runtime.json')
    assert a['result']=='PASS' and a['restored_attack_blocks_dash'] and not a['nav_path_valid']
    for item in read(D/'manifest.json'):
        p=R/'rollback_test/ThirdPerson'/item['path']
        assert hashlib.sha256(p.read_bytes()).hexdigest()==item['original_sha256']
    print('ROLLBACK: hashes=7/7; original-behavior=RESTORED; tests=5/5; exit=0')
else:raise SystemExit('Expected BASELINE, MODIFIED or ROLLBACK')
