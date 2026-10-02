import json,subprocess,sys
from pathlib import Path
root=Path.cwd();raw=root/'build/nonplanar-evidence/B13-native-lineage';exe=root/'build/arm64/src/nonplanar_verify/Release/nonplanar_rate_audit'
for name,script,count in [('rate','validate_rate_audit_cli.py',10),('material','validate_material_audit_cli.py',10),('cover','validate_material_cover_cli.py',18),('joined','validate_joined_material_cli.py',21),('nominal','validate_nominal_run_cli.py',21),('support','validate_run_support_cli.py',20)]:
 cmd=[sys.executable,'scripts/nonplanar/'+script,'--executable',str(exe),'--output-dir',str(raw/(name+'-cli'))]
 if name not in ['rate','material']:cmd+=['--fixture-dir',str(raw/'material-cli/positive')]
 assert subprocess.run([sys.executable,'scripts/nonplanar/run_logged.py','--output',str(raw/(name+'-cli.txt')),'--',*cmd]).returncode==0,name
 record=json.loads((raw/(name+'-cli')/'manifest.json').read_text());assert record['status']=='PASS' and len(record['cases'])==count
print(json.dumps({'status':'PASS','cases':100}))
