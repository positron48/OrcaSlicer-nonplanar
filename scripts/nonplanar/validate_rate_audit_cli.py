#!/usr/bin/env python3
"""Exercise native final-byte rate audit; component PASS never approves a job."""
import argparse, hashlib, json, subprocess
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--executable',type=Path,required=True)
parser.add_argument('--output-dir',type=Path,required=True)
args=parser.parse_args();exe=args.executable.resolve();root=args.output_dir.resolve();root.mkdir(parents=True,exist_ok=False)
policy={'version':1,'profile_id':1,'revision':1,'synthetic':True,'operator_confirmed_claim':False,'model':'full_stop','kinematics':'corexy',
 'initial_position':[0,0,0],'position_min':[-100]*3,'position_max':[100]*3,'axis_speed':[10,10,2],'axis_acceleration':[20,20,5],
 'drive_speed':[15,15,2],'drive_acceleration':[30,30,5],'initial_acceleration':20,'filament_diameter':1.75,'flow':1.17,
 'filament_speed':5,'filament_acceleration':10,'max_retraction':2,'max_volume_rate':2,'max_cross_section':.5,'max_event_rate':100}
body='G90\nM83\nM400\nM204 S4\nG1 X3 Y4 Z0 E.2 F300\nM400\nG1 E-.8 F120\nM400\nG1 E.8 F120\nM400\nG4 P10\nM400\n'
records=[]
for name,status,exit_code in [('positive','PASS',0),('axis-limit','FAIL',2),('missing-stop','FAIL',2),('pure-e-purge','FAIL',2),
                              ('open-pressure','PASS',0),
                              ('measured-claim','UNKNOWN',3),('duplicate-policy','UNKNOWN',3),('unknown-setting','UNKNOWN',3),('array-extra','UNKNOWN',3),('nonregular','UNKNOWN',3)]:
 work=root/name;work.mkdir();p=policy.copy();text=body
 if name=='axis-limit':p['axis_speed']=[1,1,2]
 if name=='missing-stop':text=body[:-5]
 if name=='pure-e-purge':text='G90\nM83\nM400\nM204 S4\nG1 E1 F60\nM400\n'
 if name=='open-pressure':text='G90\nM83\nM400\nM204 S4\nG1 E-.8 F120\nM400\n'
 if name=='measured-claim':p['operator_confirmed_claim']=True
 if name=='array-extra':p['axis_speed']=[10,10,2,999]
 if name=='unknown-setting':p['unmodelled_override']=2
 profile=work/'policy.json';profile.write_text(json.dumps(p))
 if name=='duplicate-policy':profile.write_text(profile.read_text()[:-1]+',"flow":2}')
 gcode=work/'candidate.txt';gcode.write_text(text)
 input_path=work if name=='nonregular' else gcode
 before={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in [profile,gcode]}
 command=[str(exe),'--linear-rates-only',str(profile),str(input_path)]
 run=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=5)
 (work/'stdout.json').write_bytes(run.stdout);(work/'stderr.txt').write_bytes(run.stderr)
 report=json.loads(run.stdout)
 assert run.returncode==exit_code,(name,run.returncode,report)
 assert report['component_status']==status and report['job_status']=='UNKNOWN' and report['export_allowed'] is False,(name,report)
 if name=='open-pressure':
  assert report['command_volume_mm3']==[0,0] and report['final_pressure_debt_mm'][0]<=.8<=report['final_pressure_debt_mm'][1]
 after={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in [profile,gcode]};assert before==after
 records.append({'case':name,'command':command,'exit_code':run.returncode,'component_status':status,'inputs_unchanged':True})
(root/'manifest.json').write_text(json.dumps({'cases':records,'status':'PASS','scope':'LINEAR_RATE_COMPONENT_ONLY_JOB_EXPORT_BLOCKED'},indent=2)+'\n')
print(json.dumps({'cases':len(records),'status':'PASS','job_export':'BLOCK'}))
