"""Submit a bounded build script to the current Unreal Editor and retain output."""
import json,pathlib,sys,time,uuid
root=pathlib.Path(__file__).resolve().parent
script=sys.argv[1]
jid=pathlib.Path(script).stem+'_'+uuid.uuid4().hex[:8]
job={'id':jid,'script':script}
if len(sys.argv)>2: job.update(json.loads(sys.argv[2]))
temp=root/'request.tmp'
temp.write_text(json.dumps(job),encoding='utf-8')
temp.replace(root/'request.json')
deadline=time.monotonic()+600
while time.monotonic()<deadline:
    try:
        data=json.loads((root/'response.json').read_text(encoding='utf-8'))
        if data.get('id')==jid:
            (root/'logs').mkdir(exist_ok=True)
            (root/'logs'/f'{jid}.json').write_text(json.dumps(data,indent=2,ensure_ascii=False),encoding='utf-8')
            print(data.get('output','').rstrip())
            if not data['success']: print(data.get('error',''),file=sys.stderr)
            sys.exit(0 if data['success'] else 1)
    except (OSError,json.JSONDecodeError): pass
    time.sleep(0.5)
print('Timed out waiting for Unreal job '+jid,file=sys.stderr)
sys.exit(2)
