from pathlib import Path
import json,sys,hashlib,zipfile
D=Path(__file__).resolve().parent;P=D.parent.parent;R=P/'Saved/RangedCombatRepair/20260906_v1';E=D/'evidence'
read=lambda n:json.loads((E/n).read_text(encoding='utf-8-sig'))
hashfile=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
m=json.loads((D/'manifest.json').read_text())
def auto(n,count):
 j=read(n+'.json');assert len(j['tests'])==count and j['failed']==0 and all(t['state']=='Success' for t in j['tests'])
def main(mode):
 if mode=='BASELINE':
  j=read('baseline_runtime.json');assert j['result']=='OBSERVED' and j['hp_before']==100 and j['hp_after']==100
  assert j['patrol_before']['pos']==j['patrol_after']['pos']
  auto('baseline_automation',8)
  for x in m:
   if x['original_sha256']:assert hashfile(D/'backup'/x['path'])==x['original_sha256']
  print('BASELINE: HP=100->100 patrol=0cm native=8/8 originals=13/13')
 elif mode=='MODIFIED':
  for n,total in [('final_runtime.json',19),('final_legacy_combo_runtime.json',13)]:
   j=read(n);assert j['result']=='PASS' and len(j['checks'])==total and all(c['pass'] for c in j['checks'])
  j=read('locked_modified.json');assert j['result']=='PASS' and len(j['cases'])==16 and all(j['checks'].values())
  auto('final_automation',13);auto('production_automation',13)
  assert read('production_smoke.json')['warp_components']==1
  assert read('production_assets.json')['result']=='PASS'
  for x in m:assert hashfile(P/x['path'])==x['modified_sha256']
  with zipfile.ZipFile(D/'MODIFIED_FILE.zip') as z:
   assert set(z.namelist())=={x['path'] for x in m}
   for x in m:assert hashlib.sha256(z.read(x['path'])).hexdigest()==x['modified_sha256']
  print('MODIFIED: native=13/13 PIE=40/40 locked=16/16 files=16/16')
 elif mode=='ROLLBACK':
  j=read('rollback_runtime.json');assert j['result']=='OBSERVED' and j['hp_before']==j['hp_after']==100
  assert j['patrol_before']['pos']==j['patrol_after']['pos'];auto('rollback_automation',8)
  assert read('rollback_candidate_hashes.json')['candidate_files_verified']==16
  for x in m:
   p=R/'rollback_test/ThirdPerson'/x['path']
   assert (hashfile(p)==x['original_sha256'] if x['original_sha256'] else not p.exists())
  assert 'ROLLBACK: restored=13 removed=3 build=Succeeded exit=0' in (E/'rollback_command.txt').read_text(encoding='utf-8-sig')
  print('ROLLBACK: restored=13 removed=3 HP=100->100 patrol=0cm native=8/8')
 else:raise ValueError('Use BASELINE, MODIFIED, or ROLLBACK')
try:main(sys.argv[1])
except Exception as e:print('VERIFY_FAILED:',repr(e));sys.exit(1)
