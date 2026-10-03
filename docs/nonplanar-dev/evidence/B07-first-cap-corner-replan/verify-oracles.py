from pathlib import Path
import json
raw=Path('build/nonplanar-evidence/B07-first-cap-corner-replan')
entries=json.loads((raw/'oracle-commands.json').read_text())
assert len(entries)==20
for row in entries:
 meta=json.loads((raw/(row['name']+'-oracle.txt.json')).read_text())
 assert meta['command']==row['command'] and meta['exit_code']==0 and meta['status']=='COMPLETED',row['name']
print(json.dumps({'status':'PASS','commands':len(entries),'failed_historical_launcher':'INVALID_PARENT_REPORT_PATH_RETAINED_AND_REPAIRED_ONLY_ONE_COMMAND_RERUN'}))
