from pathlib import Path
import concurrent.futures,json,subprocess,sys
raw=Path('build/nonplanar-evidence/B14-native-watchdog')
entries=json.loads((raw/'oracle-commands.json').read_text())
def run(row):
    code=subprocess.run([sys.executable,'scripts/nonplanar/run_logged.py','--output',str(raw/(row['name']+'-oracle.txt')),'--',*row['command']],stdout=subprocess.DEVNULL).returncode
    return row['name'],code
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    results=list(pool.map(run,entries))
print(json.dumps(results))
assert all(code==0 for _,code in results),results
