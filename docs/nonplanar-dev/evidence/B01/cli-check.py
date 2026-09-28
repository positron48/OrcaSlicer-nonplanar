import hashlib,json,subprocess,sys
from pathlib import Path
root=Path.cwd()
base=root/'build/nonplanar-evidence/B01-cli'
base.mkdir(exist_ok=False)
policy=base/'offline.sb'
policy.write_text('(version 1)\n(allow default)\n(deny network*)\n(deny file-write*)\n'+f'(allow file-write* (subpath {json.dumps(str(base))}))\n'+'(allow file-write* (literal "/dev/null"))\n')
inputs=root/'tests/nonplanar/data/baseline'
app=root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer'
records=[]
for mode,zaa in [('safe_hybrid','0'),('strict_nonplanar','0'),('future_mode','0'),('safe_hybrid','1')]:
 work=base/(mode+'-zaa'+zaa); work.mkdir()
 for name in ['data','tmp','output']: (work/name).mkdir()
 process=json.loads((inputs/'process-off.json').read_text())
 process.update(nptop_mode=mode,zaa_enabled=zaa,fuzzy_skin='disabled_fuzzy')
 settings=work/'process.json'; settings.write_text(json.dumps(process,indent=2)+'\n')
 cmd=['env','TMPDIR='+str(work/'tmp')+'/', 'sandbox-exec','-f',str(policy),str(app),'--datadir',str(work/'data'),'--debug','2','--load-settings',str(inputs/'machine.json')+';'+str(settings),'--load-filaments',str(inputs/'filament.json'),'--arrange','1','--orient','0','--outputdir',str(work/'output'),'--slice','0',str(root/'docs/nonplanar/fixtures/models/flat_block.stl')]
 result=subprocess.run([sys.executable,str(root/'scripts/nonplanar/run_logged.py'),'--output',str(work/'run.log'),'--',*cmd],cwd=work)
 log=(work/'run.log').read_text()
 outputs=list(work.rglob('*.gcode'))
 passed=result.returncode!=0 and 'Nonplanar Top Lab:' in log and not outputs
 records.append(dict(mode=mode,zaa=zaa,exit_code=result.returncode,gate_diagnostic='Nonplanar Top Lab:' in log,gcode_files=len(outputs),passed=passed))
 report=dict(binary_sha256=hashlib.sha256(app.read_bytes()).hexdigest(),cases=records)
 (base/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
 if not passed: sys.exit(1)
print(json.dumps(report,indent=2))
