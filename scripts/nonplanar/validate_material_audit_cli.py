#!/usr/bin/env python3
"""Independent material diagnostic CLI: changed final bytes must not reuse source material."""
import argparse,copy,hashlib,json,math,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True)
a=p.parse_args();exe=a.executable.resolve();root=a.output_dir.resolve();root.mkdir(parents=True,exist_ok=False)
rate={'version':1,'profile_id':11,'revision':1,'synthetic':True,'operator_confirmed_claim':False,'model':'full_stop','kinematics':'corexy','initial_position':[0,0,0],'position_min':[-100]*3,'position_max':[100]*3,'axis_speed':[10,10,2],'axis_acceleration':[20,20,5],'drive_speed':[15,15,2],'drive_acceleration':[30,30,5],'initial_acceleration':20,'filament_diameter':1.75,'flow':1.17,'filament_speed':5,'filament_acceleration':10,'max_retraction':2,'max_volume_rate':2,'max_cross_section':.5,'max_event_rate':100}
policy={'version':1,'model_id':21,'policy_id':22,'revision':1,'source_revision':7,'source_fingerprint':'a'*64,'synthetic':True,'operator_confirmed_claim':False,'outer_xy_growth_mm':.02,'outer_z_growth_mm':.01,'inner_xy_loss_mm':.03,'inner_z_loss_mm':.02,'numerical_coordinate_error_mm':1e-7,'max_coordinate_delta_mm':1e-6,'max_nominal_delta_mm3':1e-9,'max_total_nominal_delta_mm3':1e-8,'max_filament_delta_mm':1e-9,'relative_dose_error':.05,'absolute_dose_error_mm3':1e-5}
volume=.2*math.pi*1.75**2/4/1.17;pose=[3,4,.1]
events=[]
for i,kind in enumerate(['deposit','retraction','restore','dwell']):
 events.append({'event_id':i+1,'sequence_index':i,'kind':kind,'start':[0,0,0] if i==0 else pose,'end':pose,'expected_nominal_volume_mm3':volume if i==0 else 0,'expected_filament_mm':.8 if i in [1,2] else 0,'section':{'kind':'rectangle','gap_begin_mm':.2,'gap_end_mm':.3} if i==0 else None})
base={'version':1,'policy':policy,'events':events}
body='G90\nM83\nM400\nM204 S4\nG1 X3 Y4 Z.1 E.2 F60\nM400\nG1 E-.8 F120\nM400\nG1 E.8 F120\nM400\nG4 P10\nM400\n'
records=[]
for name,status,code in [('positive','PASS',0),('changed-e','FAIL',2),('changed-xyz','FAIL',2),('missing-owner','FAIL',2),('duplicate-owner','UNKNOWN',3),('extra-field','UNKNOWN',3),('measured-claim','UNKNOWN',3),('bad-array','UNKNOWN',3),('changed-pressure','FAIL',2),('duplicate-key','UNKNOWN',3)]:
 work=root/name;work.mkdir();m=copy.deepcopy(base);text=body
 if name=='changed-e':text=text.replace('E.2 F60','E.3 F60')
 if name=='changed-xyz':text=text.replace('X3 Y4','X3.01 Y4')
 if name=='missing-owner':m['events'].pop()
 if name=='duplicate-owner':m['events'][1]['event_id']=1
 if name=='extra-field':m['policy']['collision_free']=True
 if name=='measured-claim':m['policy']['operator_confirmed_claim']=True
 if name=='bad-array':m['events'][0]['start'].append(1)
 if name=='changed-pressure':m['events'][1]['expected_filament_mm']=.9
 rp=work/'rate.json';mp=work/'material.json';gp=work/'candidate.txt';rp.write_text(json.dumps(rate));mp.write_text(json.dumps(m));gp.write_text(text)
 if name=='duplicate-key':mp.write_text(mp.read_text()[:-1]+',"version":1}')
 files=[rp,mp,gp];before={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
 cmd=[str(exe),'--linear-material-only',str(rp),str(mp),str(gp)];run=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=5)
 (work/'stdout.json').write_bytes(run.stdout);(work/'stderr.txt').write_bytes(run.stderr);report=json.loads(run.stdout)
 assert run.returncode==code,(name,run.returncode,report)
 assert report['component_status']==status and report['job_status']=='UNKNOWN' and report['export_allowed'] is False,(name,report)
 if name=='positive':assert report['records']==4 and report['depositions']==1 and report['nominal_volume_mm3'][0]<=volume<=report['nominal_volume_mm3'][1]
 assert before=={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
 records.append({'case':name,'command':cmd,'exit_code':run.returncode,'component_status':status,'inputs_unchanged':True})
(root/'manifest.json').write_text(json.dumps({'cases':records,'status':'PASS','scope':'DECLARED_MATERIAL_COMPONENT_ONLY_JOB_EXPORT_BLOCKED'},indent=2)+'\n');print(json.dumps({'cases':len(records),'status':'PASS','job_export':'BLOCK'}))
