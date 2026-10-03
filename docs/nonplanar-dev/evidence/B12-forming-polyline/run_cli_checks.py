from pathlib import Path
import json,subprocess
raw=Path('build/nonplanar-evidence/B12-forming-polyline')
for item in json.loads((raw/'cli-plan.json').read_text()):
 subprocess.run(['python3','scripts/nonplanar/run_logged.py','--output',str(raw/(item['name']+'.txt')),'--',*item['argv']],check=True)
