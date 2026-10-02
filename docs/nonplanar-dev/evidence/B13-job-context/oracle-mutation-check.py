from pathlib import Path
import copy, hashlib, json, subprocess
root=Path.cwd();raw=root/'build/nonplanar-evidence/B13-job-context';dest=raw/'oracle-mutations';dest.mkdir(exist_ok=False)
records=[]
def run(name,kind,record,expected):
 path=dest/(name+'.json');path.write_text(json.dumps(record,ensure_ascii=False)+'\n')
 command=['python3','scripts/nonplanar/job_'+kind+'_oracle.py','--input',str(path)]
 result=subprocess.run(command,capture_output=True,timeout=10)
 (dest/(name+'.stdout.txt')).write_bytes(result.stdout);(dest/(name+'.stderr.txt')).write_bytes(result.stderr)
 assert result.returncode==expected,(name,result.returncode)
 records.append({'case':name,'command':command,'exit_code':result.returncode})
context=json.loads((raw/'job-fixtures/native-job.json').read_text())
artifact=json.loads((raw/'job-fixtures/bound-candidate.json').read_text())
run('context-positive','context',context,0);run('artifact-positive','artifact',artifact,0)
maximum=copy.deepcopy(context);maximum['job_id']=2**64-1
canonical=json.loads(maximum['canonical']);canonical['job_id']=maximum['job_id']
maximum['canonical']=json.dumps(canonical,sort_keys=True,separators=(',',':'))
maximum['fingerprint']=hashlib.sha256(maximum['canonical'].encode()).hexdigest()
run('exact-uint64-max','context',maximum,0)
for field in ['job_id','input_revision','native_input_identity_fingerprint','executed_input_identity','print_settings_identity','native_input_canonical']:
 record=copy.deepcopy(context);value=record[field];record[field]=value+1 if isinstance(value,int) else value+' '
 run('context-'+field,'context',record,1)
for field in ['bytes','name','kind']:
 record=copy.deepcopy(context);value=record['resources'][0][field];record['resources'][0][field]=value+1 if isinstance(value,int) else value+'x'
 run('resource-'+field,'context',record,1)
for field in ['candidate_bytes','job_canonical','attempt','job_id','material_journal','motion_policy','serializer_policy','source_fingerprint','source_revision']:
 record=copy.deepcopy(artifact);value=record[field];record[field]=value+1 if isinstance(value,int) else value+'x'
 run('artifact-'+field,'artifact',record,1)
record=copy.deepcopy(artifact);record['initial_position'][0]+=.1;run('artifact-initial-position','artifact',record,1)
record=copy.deepcopy(artifact);manifest=json.loads(record['manifest']);manifest['candidate_size']+=1
record['manifest']=json.dumps(manifest,sort_keys=True,separators=(',',':'))
record['manifest_sha256']=hashlib.sha256(record['manifest'].encode()).hexdigest();run('rehash-wrong-manifest','artifact',record,1)
(dest/'manifest.json').write_text(json.dumps({'status':'PASS','cases':records,'scope':'IDENTITY_ONLY_NO_GEOMETRY_OR_PUBLICATION_CERTIFICATE'},indent=2)+'\n')
print(json.dumps({'status':'PASS','positive':3,'refusals':len(records)-3}))
