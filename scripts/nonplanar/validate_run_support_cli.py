#!/usr/bin/env python3
"""Final-byte whole-run support; future material and declared gaps cannot grant PASS."""
import argparse,copy,hashlib,json,math,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--fixture-dir',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True)
a=p.parse_args();exe=a.executable.resolve();root=a.output_dir.resolve();root.mkdir(parents=True,exist_ok=False)
rate=json.loads((a.fixture_dir/'rate.json').read_text());material=json.loads((a.fixture_dir/'material.json').read_text())
join={'version':1,'policy_id':31,'revision':1,'synthetic':True,'operator_confirmed_claim':False,'model':'common_run_envelope'}
base={'version':1,'completed_records':3,'current_progress':0,'run_index':1,'policy':{'version':1,'policy_id':41,'revision':1,'synthetic':True,'operator_confirmed_claim':False,'cross_slope':.1,'vertical_min':.14,'vertical_max':.26,'normal_min':.14,'normal_max':.26}}
cases=[('positive','PASS',0),('rotated','PASS',0),('reverse','PASS',0),('partial','PASS',0),('pressure','PASS',0),('future','FAIL',2),('narrow-lower','FAIL',2),('gap-large','FAIL',2),('gap-small','FAIL',2),('changed-e','FAIL',2),('bad-band','UNKNOWN',3),('measured-claim','UNKNOWN',3),('wrong-version','UNKNOWN',3),('extra-field','UNKNOWN',3),('duplicate-key','UNKNOWN',3),('nested-array','UNKNOWN',3),('bad-run','UNKNOWN',3),('negative-count','UNKNOWN',3),('oversized-count','UNKNOWN',3),('bad-join','UNKNOWN',3)]
records=[]
for name,status,code in cases:
 work=root/name;work.mkdir();m=copy.deepcopy(material);q=copy.deepcopy(base);j=copy.deepcopy(join);rows=[];lines=['G90','M83','M400','M204 S4'];previous=[0,0,0]
 def pose(t,z):return [(-1 if name=='reverse' else 1)*(.6 if name=='rotated' else 1)*t,.8*t if name=='rotated' else 0,z]
 def append(end,e,kind=None):
  event_kind=kind or ('deposit' if e else 'travel');command='G1'
  if event_kind in ['deposit','travel']:command+=f' X{end[0]:.9f} Y{end[1]:.9f} Z{end[2]:.9f}'
  if e:command+=f' E{e:.9f}'
  command+=' F30';lines.extend([command,'M400'])
  rows.append({'event_id':len(rows)+1,'sequence_index':len(rows),'kind':event_kind,'start':list(previous),'end':list(end),
   'expected_nominal_volume_mm3':e*math.pi*rate['filament_diameter']**2/4/rate['flow'] if event_kind=='deposit' else 0,
   'expected_filament_mm':abs(e) if event_kind in ['retraction','restore'] else 0,'section':{'kind':'rectangle','gap_begin_mm':.2,'gap_end_mm':.2} if event_kind=='deposit' else None})
 append(pose(3,0),0 if name=='future' else .0001 if name=='narrow-lower' else .2);previous=pose(3,0)
 if name=='pressure':append(previous,-.01,'retraction');append(previous,.01,'restore')
 z=.5 if name=='gap-large' else .05 if name=='gap-small' else .2;append(pose(.5,z),0);previous=pose(.5,z);append(pose(2.5,z+.02),.03);previous=pose(2.5,z+.02)
 if name=='future':append(pose(0,0),0);previous=pose(0,0);append(pose(3,0),.2);q['run_index']=0
 m['events']=rows;q['completed_records']=len(rows);body='\n'.join(lines)+'\n'
 if name=='partial':q['completed_records']=2;q['current_progress']=.5
 if name=='changed-e':body=body.replace('E0.200000000','E0.300000000')
 if name=='bad-band':q['policy']['normal_min']=.3
 if name=='measured-claim':q['policy']['operator_confirmed_claim']=True
 if name=='wrong-version':q['policy']['version']=2
 if name=='extra-field':q['policy']['supported']=True
 if name=='nested-array':q['policy']['cross_slope']=[[0]]
 if name=='bad-run':q['run_index']=200000
 if name=='negative-count':q['completed_records']=-1
 if name=='oversized-count':q['completed_records']=4294967296
 if name=='bad-join':j['operator_confirmed_claim']=True
 for filename,value in [('rate.json',rate),('material.json',m),('join.json',j),('query.json',q)]:
  (work/filename).write_text(json.dumps(value))
 (work/'candidate.txt').write_text(body)
 if name=='duplicate-key':f=work/'query.json';f.write_text(f.read_text().replace('"normal_min": 0.14','"normal_min": 0.14, "normal_min": 0.14'))
 files=[work/f for f in ['rate.json','material.json','join.json','query.json','candidate.txt']];before={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
 cmd=[str(exe),'--linear-run-support-only',*[str(f) for f in files]];run=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=5)
 (work/'stdout.json').write_bytes(run.stdout);(work/'stderr.txt').write_bytes(run.stderr);report=json.loads(run.stdout)
 assert run.returncode==code and report['component_status']==status,(name,run.returncode,report)
 assert report['component']=='final_byte_run_support' and report['job_status']=='UNKNOWN' and report['export_allowed'] is False,(name,report)
 if status=='PASS':assert report['support_leaves']>0 and report['lower_anchor_leaves']>0 and report['nominal_terminal_leaves']>0
 if name in ['future','narrow-lower','gap-large','gap-small']:assert report['witness'] is not None
 assert before=={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
 records.append({'case':name,'command':cmd,'exit_code':run.returncode,'component_status':status,'inputs_unchanged':True})
(root/'manifest.json').write_text(json.dumps({'cases':records,'status':'PASS','scope':'ACTUAL_RUN_NOMINAL_GAPS_AND_LOWER_ANCHORS_ONLY_JOB_EXPORT_BLOCKED'},indent=2)+'\n');print(json.dumps({'cases':len(records),'status':'PASS','export':'BLOCK'}))
