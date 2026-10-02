from pathlib import Path
import gzip,hashlib,json
root=Path.cwd();raw=root/'build/nonplanar-evidence/B13-native-departure'
parent=json.loads((root/'docs/nonplanar-dev/evidence/B09-native-departure/source-manifest.json').read_text())
def sha(data):return hashlib.sha256(data).hexdigest()
def archived(path):
 p=root/parent['raw_to_archive'][path]
 return gzip.decompress(p.read_bytes()) if p.suffix=='.gz' else p.read_bytes()
files={}
for path in parent['raw_to_archive']:
 if '/final-native2/' not in path:continue
 name=Path(path).name
 files[name]={'parent':sha(archived(path)),'current':sha((raw/'ctest-native-evidence'/name).read_bytes())}
old='build/nonplanar-evidence/B09-native-departure/final-job2/native-lineage-report.json'
files['native-lineage-report.json']={'parent':sha(archived(old)),'current':sha((raw/'ctest-job-evidence/native-lineage-report.json').read_bytes())}
differences=[name for name,pair in files.items() if pair['parent']!=pair['current']]
assert not differences,differences
record=json.loads((raw/'ctest-job-evidence/native-departure-report.json').read_text())
assert record==json.loads((raw/'focus-evidence/native-departure-report.json').read_text())
n=record['native'];departure=json.loads(n['departure_canonical']);plan=json.loads(n['canonical']);manifest=json.loads(record['manifest'])
assert plan['schema']==2 and manifest['schema']==3
checks=json.loads(record['canonical'])['validation']['checks'];assert len(checks)==17
assert sum(c['status']=='PASS' and c['execution']=='RUN' for c in checks)==4
assert sum(c['status']=='UNKNOWN' and c['execution']=='NOT_RUN' for c in checks)==13
result={'status':'PASS_EXACT_ORIGINAL_OUTPUT_AND_NEW_FOCUS_CTEST_IDENTITY_ONLY','original_files':files,'differences':differences,
 'native_departure':{'journals':{k:len(n[k]['records']) for k in ['body','before','assembled','routed','planned']},
 'candidate_bytes':len(record['candidate_bytes'].encode()),'candidate_sha256':record['candidate_sha256'],
 'departure_sha256':n['departure_sha256'],'native_plan_sha256':n['sha256'],'manifest_sha256':record['manifest_sha256'],
 'report_sha256':record['sha256'],'legs':departure['legs'],'mandatory':17,'pass_run':4,'unknown_not_run':13,'export':'BLOCK'}}
(raw/'provenance-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
