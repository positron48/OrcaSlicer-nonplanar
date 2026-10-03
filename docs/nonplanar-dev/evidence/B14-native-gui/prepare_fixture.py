from pathlib import Path
import json, shutil, subprocess, sys
root=Path.cwd();raw=root/'build/nonplanar-evidence/B14-native-gui';work=raw/'gui-fixture'
work.mkdir();old=root/'build/nonplanar-evidence/B14-native-worker/cli0/complete-blocked'
for folder in ['data','tmp','output']:(work/folder).mkdir()
for name in ['machine.json','process.json','filament.json','request.json']:
 value=json.loads((old/name).read_text())
 if name=='process.json':value['nptop_mode']='off'
 (work/name).write_text(json.dumps(value)+'\n')
policy=work/'offline.sb';policy.write_text('(version 1)\n(allow default)\n(deny network*)\n(deny file-write*)\n'+f'(allow file-write* (subpath {json.dumps(str(work))}))\n'+'(allow file-write* (literal "/dev/null"))\n')
command=json.loads((old/'command.json').read_text())['command']
command=[arg.replace(str(old),str(work)) for arg in command]
i=command.index('--nptop-analyze');command[i:i+2]=['--export-3mf','simulation-source.3mf']
command=['env','TMPDIR='+str(work/'tmp')+'/',*command]
result=subprocess.run([sys.executable,'scripts/nonplanar/run_logged.py','--output',str(raw/'gui-fixture.txt'),'--',*command])
raise SystemExit(result.returncode)
