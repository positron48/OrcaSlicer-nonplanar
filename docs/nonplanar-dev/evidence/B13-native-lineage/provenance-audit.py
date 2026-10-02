import hashlib,json
from pathlib import Path
raw=Path('build/nonplanar-evidence/B13-native-lineage');old=Path('build/nonplanar-evidence/B13-job-context/native-final4');current=raw/'final-ctest-native'
files={};differences=[]
for p in sorted(old.iterdir()):
 if not p.is_file():continue
 q=current/p.name;assert q.is_file();same=p.read_bytes()==q.read_bytes();files[p.name]={'same_bytes':same,'old_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'current_sha256':hashlib.sha256(q.read_bytes()).hexdigest()}
 if not same:
  assert p.name=='native-material.json',p.name
  a=json.loads(p.read_text());b=json.loads(q.read_text());a['policy'].pop('source_fingerprint');b['policy'].pop('source_fingerprint');assert a==b
  differences.append('native-material.json: source_fingerprint only; every other JSON field exact')
assert len(files)==11
report=json.loads((current/'native-job-report.json').read_text());assert report['records']==2218 and report['candidate_sha256']=='d31bc1ef4ec3c4c1342d0ec2ecd89818749acf52a33ce77a1f149d32f32220a8'
record={'status':'PASS_ORIGINAL_NATIVE_CONTENT_RETAINED_STRICT_SOURCE_IDENTITY_NOT_NORMALIZED','original_files':files,'differences':differences,'original_candidate_sha256':report['candidate_sha256'],'records':report['records']}
(raw/'provenance-comparison.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record))
