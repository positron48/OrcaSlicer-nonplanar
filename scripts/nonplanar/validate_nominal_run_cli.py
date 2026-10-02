#!/usr/bin/env python3
"""Actual nominal section union at exact packet cuts; no inferred Lower support."""
import argparse,copy,hashlib,json,math,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--fixture-dir',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True)
a=p.parse_args();exe=a.executable.resolve();root=a.output_dir.resolve();root.mkdir(parents=True,exist_ok=False)
rate=json.loads((a.fixture_dir/'rate.json').read_text());material=json.loads((a.fixture_dir/'material.json').read_text())
join={'version':1,'policy_id':31,'revision':1,'synthetic':True,'operator_confirmed_claim':False,'model':'common_run_envelope'}
query={'version':1,'completed_records':5,'current_progress':0,'representation':'nominal','region_min':[.059,-.001,-.072],'region_max':[.061,.001,-.068]}
cases=[('positive','PASS',0),('partial-interior','PASS',0),('front','FAIL',2),('future','FAIL',2),('pause','UNKNOWN',3),('pressure','UNKNOWN',3),('turn','UNKNOWN',3),('narrow-dose','FAIL',2),('raised-floor','FAIL',2),('changed-e','FAIL',2),('wrong-version','UNKNOWN',3),('measured-claim','UNKNOWN',3),('not-synthetic','UNKNOWN',3),('unknown-model','UNKNOWN',3),('extra-field','UNKNOWN',3),('duplicate-key','UNKNOWN',3),('nested-policy','UNKNOWN',3),('bad-id','UNKNOWN',3),('nonnominal','UNKNOWN',3),('extra-query','UNKNOWN',3),('partial-after-end','UNKNOWN',3)]
records=[]
for name,status,code in cases:
 work=root/name;work.mkdir();m=copy.deepcopy(material);j=copy.deepcopy(join);q=copy.deepcopy(query);rows=[];lines=['G90','M83','M400','M204 S4'];previous=[0,0,0]
 def append(kind,end,e,command,section=None):
  rows.append({'event_id':len(rows)+1,'sequence_index':len(rows),'kind':kind,'start':list(previous),'end':list(end),
   'expected_nominal_volume_mm3':e*math.pi*rate['filament_diameter']**2/4/rate['flow'] if kind=='deposit' else 0,
   'expected_filament_mm':abs(e) if kind in ['retraction','restore'] else 0,'section':section});lines.extend([command,'M400'])
 for i in range(1,6):
  if i==4 and name=='pause':append('dwell',previous,0,'G4 P10')
  if i==4 and name=='pressure':
   append('retraction',previous,-.01,'G1 E-.01 F60');append('restore',previous,.01,'G1 E.01 F60')
  end=[.02*i,.000001 if name=='turn' and i>=4 else 0,.01*i];e=.0001 if name=='narrow-dose' and i==3 else .001;gap=.05 if name=='raised-floor' and i==3 else .2
  append('deposit',end,e,f'G1 X{end[0]:.6f} Y{end[1]:.6f} Z{end[2]:.6f} E{e:.6f} F30',{'kind':'rectangle','gap_begin_mm':gap,'gap_end_mm':gap});previous=end
 m['events']=rows;q['completed_records']=len(rows);body='\n'.join(lines)+'\n'
 if name=='narrow-dose':q['region_min']=[.049,.039,-.077];q['region_max']=[.051,.041,-.073]
 if name=='raised-floor':q['region_min']=[.049,-.001,-.077];q['region_max']=[.051,.001,-.073]
 if name=='partial-interior':q['completed_records']=4;q['current_progress']=.5
 if name=='front':q['completed_records']=2;q['current_progress']=.5
 if name=='future':q['completed_records']=0
 if name=='changed-e':body=body.replace('E0.001000 F30','E0.002000 F30')
 if name=='wrong-version':j['version']=2
 if name=='measured-claim':j['operator_confirmed_claim']=True
 if name=='not-synthetic':j['synthetic']=False
 if name=='unknown-model':j['model']='independent_displacements'
 if name=='extra-field':j['bonded']=True
 if name=='nested-policy':j['model']={'model':'common_run_envelope'}
 if name=='bad-id':j['policy_id']=1.5
 if name=='nonnominal':q['representation']='lower'
 if name=='extra-query':q['support_pass']=True
 if name=='partial-after-end':q['current_progress']=.1
 for filename,value in [('rate.json',rate),('material.json',m),('join.json',j),('query.json',q)]:
  (work/filename).write_text(json.dumps(value))
 (work/'candidate.txt').write_text(body)
 if name=='duplicate-key':f=work/'join.json';f.write_text(f.read_text()[:-1]+',"version":1}')
 files=[work/f for f in ['rate.json','material.json','join.json','query.json','candidate.txt']];before={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
 cmd=[str(exe),'--linear-material-nominal-run-cover-only',*[str(f) for f in files]];run=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=5)
 (work/'stdout.json').write_bytes(run.stdout);(work/'stderr.txt').write_bytes(run.stderr);report=json.loads(run.stdout)
 assert run.returncode==code and report['component_status']==status,(name,run.returncode,report)
 assert report['component']=='final_byte_nominal_run_region_cover' and report['job_status']=='UNKNOWN' and report['export_allowed'] is False,(name,report)
 if status=='PASS':assert report['cover_leaves']>0 and report['owners'] and report['runs']==1 and report['joined_policy']==join
 if name in ['front','future','narrow-dose','raised-floor']:assert report['uncovered'] is not None
 assert before=={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
 records.append({'case':name,'command':cmd,'exit_code':run.returncode,'component_status':status,'inputs_unchanged':True})
(root/'manifest.json').write_text(json.dumps({'cases':records,'status':'PASS','scope':'ACTUAL_NOMINAL_RUN_ONLY_JOB_EXPORT_BLOCKED'},indent=2)+'\n');print(json.dumps({'cases':len(records),'status':'PASS','export':'BLOCK'}))
