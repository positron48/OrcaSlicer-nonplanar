from pathlib import Path
import json
r=Path('build/nonplanar-evidence/B07-roof-chord');e=json.loads((r/'oracle-commands.json').read_text())
for x in e:
 j=json.loads((r/(x['name']+'-oracle.txt.json')).read_text());assert j['status']=='COMPLETED' and j['exit_code']==0 and j['command']==x['command'],x['name']
assert len(e)==24
print(json.dumps({'status':'PASS','commands':len(e),'retained_successes':23,'corrected_historical_parent_path':1}))
