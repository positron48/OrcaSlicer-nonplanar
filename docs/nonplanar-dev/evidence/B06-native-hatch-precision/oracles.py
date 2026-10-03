from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import json,subprocess
raw=Path('build/nonplanar-evidence/B06-native-hatch-precision')
entries=json.loads((raw/'oracle-commands.json').read_text())
def run(row):
 subprocess.run(['python3','scripts/nonplanar/run_logged.py','--output',str(raw/(row['name']+'-oracle.txt')),'--',*row['command']],check=True)
with ThreadPoolExecutor(max_workers=4) as pool:list(pool.map(run,entries))
print(json.dumps({'status':'PASS','commands':len(entries)}))
