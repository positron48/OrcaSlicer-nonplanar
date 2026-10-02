#!/usr/bin/env python3
"""Final-byte whole-region cover: future material and AABB corners cannot prove coverage."""
import argparse,copy,hashlib,json,shutil,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--executable',type=Path,required=True);p.add_argument('--fixture-dir',type=Path,required=True);p.add_argument('--output-dir',type=Path,required=True)
a=p.parse_args();root=a.output_dir.resolve();root.mkdir(parents=True,exist_ok=False);exe=a.executable.resolve()
base={'version':1,'completed_records':1,'current_progress':0,'representation':'lower','region_min':[1.49,1.99,-.08],'region_max':[1.51,2.01,-.07]}
cases=[('positive','PASS',0),('upper','PASS',0),('nominal','PASS',0),('partial','PASS',0),('future','FAIL',2),('unlaid','FAIL',2),('bounding-corner','FAIL',2),('extra-field','UNKNOWN',3),('bad-representation','UNKNOWN',3),('negative-count','UNKNOWN',3),('float-count','UNKNOWN',3),('bad-array','UNKNOWN',3),('wrong-version','UNKNOWN',3),('duplicate-key','UNKNOWN',3),('after-complete','UNKNOWN',3),('changed-e','FAIL',2),('nested-array','UNKNOWN',3),('oversized-count','UNKNOWN',3)]
records=[]
for name,status,code in cases:
 work=root/name;work.mkdir();query=copy.deepcopy(base)
 for f in ['rate.json','material.json','candidate.txt']:shutil.copyfile(a.fixture_dir/f,work/f)
 if name in ['upper','nominal']:query['representation']=name
 if name in ['partial','future','unlaid']:query['completed_records']=0;query['current_progress']=.5 if name!='unlaid' else 0
 if name=='partial':query['region_min']=[.995,1.328,-.10];query['region_max']=[1.005,1.338,-.09]
 if name=='future':query['region_min']=[2.39,3.19,-.04];query['region_max']=[2.41,3.21,-.03]
 if name=='bounding-corner':query['region_min']=[.05,3.8,-.08];query['region_max']=[.06,3.81,-.07]
 if name=='extra-field':query['support_pass']=True
 if name=='bad-representation':query['representation']='filled_aabb'
 if name=='negative-count':query['completed_records']=-1
 if name=='float-count':query['completed_records']=1.5
 if name=='bad-array':query['region_min'].append(0)
 if name=='wrong-version':query['version']=2
 if name=='nested-array':query['region_min']=[[[0]],0,0]
 if name=='oversized-count':query['completed_records']=4294967296
 if name=='after-complete':query['completed_records']=4;query['current_progress']=.1
 if name=='changed-e':f=work/'candidate.txt';f.write_text(f.read_text().replace('E.2 F60','E.3 F60'))
 qp=work/'query.json';qp.write_text(json.dumps(query))
 if name=='duplicate-key':qp.write_text(qp.read_text()[:-1]+',"version":1}')
 files=[work/f for f in ['rate.json','material.json','query.json','candidate.txt']];before={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
 cmd=[str(exe),'--linear-material-cover-only',*[str(f) for f in files]];run=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=5)
 (work/'stdout.json').write_bytes(run.stdout);(work/'stderr.txt').write_bytes(run.stderr);report=json.loads(run.stdout)
 assert run.returncode==code,(name,run.returncode,report)
 assert report['component_status']==status and report['job_status']=='UNKNOWN' and report['export_allowed'] is False,(name,report)
 assert report['component']=='final_byte_material_region_cover',(name,report)
 if status=='PASS':assert report['cover_leaves']>0
 if name in ['future','unlaid','bounding-corner']:assert report['uncovered'] is not None
 assert before=={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
 records.append({'case':name,'command':cmd,'exit_code':run.returncode,'component_status':status,'inputs_unchanged':True})
(root/'manifest.json').write_text(json.dumps({'cases':records,'status':'PASS','scope':'ACTUAL_PREFIX_REGION_GEOMETRY_ONLY_JOB_EXPORT_BLOCKED'},indent=2)+'\n');print(json.dumps({'cases':len(records),'status':'PASS','export':'BLOCK'}))
