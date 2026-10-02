from pathlib import Path
import hashlib,json
root=Path.cwd();stage=root/'build/nonplanar-evidence/B09-exit-boundary';parent=root/'build/nonplanar-evidence/B13-native-lineage'
keys=json.loads((parent/'provenance-comparison.json').read_text())['original_files']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
files={k:{'parent':sha(parent/'final-ctest-native'/k),'current':sha(stage/'final-ctest-native'/k)} for k in keys}
for extra in ('native-provenance.json','native-partition-meshes.json','native-job-report.json'):
 files[extra]={'parent':sha(parent/'final-ctest-native'/extra),'current':sha(stage/'final-ctest-native'/extra)}
differences=[k for k,v in files.items() if v['parent']!=v['current']]
assert not differences,differences
new=json.loads((stage/'final-job-fixtures/native-lineage-report.json').read_text());old=json.loads((parent/'final-job-fixtures/native-lineage-report.json').read_text())
assert new['candidate_bytes']==old['candidate_bytes'] and new['candidate_sha256']==old['candidate_sha256']
checks=json.loads(new['canonical'])['validation']['checks'];assert len(checks)==17
assert sum(c['status']=='PASS' and c['execution']=='RUN' for c in checks)==4
assert sum(c['status']=='UNKNOWN' and c['execution']=='NOT_RUN' for c in checks)==13
result={'original_files':files,'differences':differences,'original_candidate_bytes':len((stage/'final-ctest-native/native-full-stop.candidate.txt').read_bytes()),'original_candidate_sha256':sha(stage/'final-ctest-native/native-full-stop.candidate.txt'),'native_lineage_records':new['records'],'native_lineage_bytes':len(new['candidate_bytes']),'native_lineage_candidate_sha256':new['candidate_sha256'],'mandatory':17,'pass_run':4,'unknown_not_run':13,'export':'BLOCK'}
(stage/'provenance-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
