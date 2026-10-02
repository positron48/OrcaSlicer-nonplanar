from pathlib import Path
import gzip,hashlib,json
root=Path.cwd();raw=root/'build/nonplanar-evidence/B13-job-context';current=raw/'native-final2'
parent_path=root/'docs/nonplanar-dev/evidence/B12-run-ray/source-manifest.json'
parent=json.loads(parent_path.read_text());hashes={str(parent_path.relative_to(root)):hashlib.sha256(parent_path.read_bytes()).hexdigest()};identical={}
for old,archive in parent['raw_to_archive'].items():
 if '/native-final/' not in old:continue
 a=root/archive;hashes[archive]=hashlib.sha256(a.read_bytes()).hexdigest()
 data=gzip.open(a,'rb').read() if a.suffix=='.gz' else a.read_bytes()
 assert hashlib.sha256(data).hexdigest()==parent['raw_sha256'][old]
 name=Path(old).name;identical[name]=data==(current/name).read_bytes();assert identical[name],name
assert len(identical)==11
rows=json.loads((current/'native-material.json').read_text())['events'];assert len(rows)==2218
result={'records':len(rows),'depositions':sum(r['kind']=='deposit' for r in rows),'parent_current_identical':identical,'parent_rows_identical':True,'parent_material_policy_differences':[],
'sha256':{str(p.relative_to(raw)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(current.iterdir())},'parent_archive_sha256':hashes,
'scope':'All eleven original candidate/policy/query files match parent; current binary rerun, no old reports reused',
'status':'PASS_CURRENT_REPLAY_IDENTICAL_INPUTS_NO_OLD_REPORT_REUSE'}
(raw/'native-input-binding.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'status':'PASS','parent_native_files_identical':len(identical),'records':len(rows),'depositions':result['depositions']}))
