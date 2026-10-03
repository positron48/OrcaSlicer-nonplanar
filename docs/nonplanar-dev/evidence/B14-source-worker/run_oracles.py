from pathlib import Path
import concurrent.futures,json,subprocess,sys
raw=Path('build/nonplanar-evidence/B14-source-worker')
old=Path('build/nonplanar-evidence/B14-native-watchdog/oracle-commands.json')
entries=json.loads(old.read_text())
for row in entries:
 row['command']=[s.replace('B14-native-watchdog','B14-source-worker') for s in row['command']]
entries[0]['command'] += ['--job-report',str(raw/'final-jobs/native-source-reference-report.json')]
entries.append({'name':'native-source','command':['python3','scripts/nonplanar/native_source_oracle.py','--fixtures',str(raw/'final-jobs')]})
(raw/'oracle-commands.json').write_text(json.dumps(entries,indent=2)+'\n')
def run(row):
 code=subprocess.run([sys.executable,'scripts/nonplanar/run_logged.py','--output',str(raw/(row['name']+'-oracle.txt')),'--',*row['command']],stdout=subprocess.DEVNULL).returncode
 return row['name'],code
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:results=list(pool.map(run,entries))
print(json.dumps(results))
assert all(code==0 for _,code in results),results
