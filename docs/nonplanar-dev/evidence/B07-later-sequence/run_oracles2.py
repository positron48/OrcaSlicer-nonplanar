from pathlib import Path
import concurrent.futures,json,subprocess,sys
raw=Path('build/nonplanar-evidence/B07-later-sequence')
old=Path('build/nonplanar-evidence/B14-source-worker/oracle-commands.json')
entries=json.loads(old.read_text())
for row in entries:
 row['command']=[s.replace('B14-source-worker','B07-later-sequence').replace('final-jobs','final-jobs2').replace('final-candidates','final-candidates2') for s in row['command']]
(raw/'oracle2-commands.json').write_text(json.dumps(entries,indent=2)+'\n')
def run(row):
 code=subprocess.run([sys.executable,'scripts/nonplanar/run_logged.py','--output',str(raw/(row['name']+'-oracle2.txt')),'--',*row['command']],stdout=subprocess.DEVNULL).returncode
 return row['name'],code
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:results=list(pool.map(run,entries))
print(json.dumps(results))
assert all(code==0 for _,code in results),results
