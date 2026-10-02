from pathlib import Path
import gzip, hashlib, json
root=Path.cwd();raw=root/'build/nonplanar-evidence/B12-final-travel'
parent=json.loads((root/'docs/nonplanar-dev/evidence/B13-native-departure/source-manifest.json').read_text())
def sha(data):return hashlib.sha256(data).hexdigest()
files={}
for path,archive in parent['raw_to_archive'].items():
 if '/final-native-evidence/' in path:current=raw/'ctest-final2-native-evidence'/Path(path).name
 elif path.endswith('/final-job-evidence/native-lineage-report.json'):current=raw/'ctest-final2-job-evidence/native-lineage-report.json'
 else:continue
 p=root/archive;data=gzip.decompress(p.read_bytes()) if p.suffix=='.gz' else p.read_bytes()
 files[current.name]={'parent':sha(data),'current':sha(current.read_bytes())}
assert len(files)==19,len(files)
differences=[name for name,pair in files.items() if pair['parent']!=pair['current']]
assert not differences,differences
new=raw/'ctest-final2-native-evidence';focused=raw/'final-focus-native-evidence'
inputs={}
for suffix in ['candidate.txt','rate.json','query.json','material.json']:
 name='native-final-travel.'+suffix
 inputs[name]={'focus':sha((focused/name).read_bytes()),'ctest':sha((new/name).read_bytes())}
assert all(v['focus']==v['ctest'] for k,v in inputs.items() if not k.endswith('material.json'))
a=json.loads((focused/'native-final-travel.material.json').read_text());b=json.loads((new/'native-final-travel.material.json').read_text())
assert a['events']==b['events']
ap=dict(a['policy']);bp=dict(b['policy']);ap.pop('source_fingerprint');bp.pop('source_fingerprint');assert ap==bp
proof=json.loads((new/'native-final-travel.proof.json').read_text());fp=json.loads((focused/'native-final-travel.proof.json').read_text());assert proof==fp
assert proof['prefix_completed_records']==2202 and proof['previous_deposits']==2076
assert proof['cells']==133 and len(proof['leaves'])==77 and proof['export_allowed'] is False
candidate=(new/'native-final-travel.candidate.txt').read_bytes();assert sha(candidate)=='b8eb15681127c1a8db57ce0aa257326763f16832b82fdafd13ed091c4bd98ef8'
report=json.loads((raw/'ctest-final2-job-evidence/native-departure-report.json').read_text())
assert report['candidate_bytes'].encode()==candidate
checks=json.loads(report['canonical'])['validation']['checks'];assert len(checks)==17
assert sum(c['status']=='PASS' and c['execution']=='RUN' for c in checks)==4
assert sum(c['status']=='UNKNOWN' and c['execution']=='NOT_RUN' for c in checks)==13
result={'status':'PASS_EXACT_OLD_FILES_AND_ACTUAL_NEW_BYTES_SCENE_POLICY_LEAVES_ONLY',
 'original_files':files,'differences':differences,'new_inputs':inputs,'actual_event_rows':'EXACT',
 'material_source_fingerprints':{'focus':a['policy']['source_fingerprint'],'ctest':b['policy']['source_fingerprint'],
 'scope':'COMPLETE_SOURCE_PROVENANCE_UNQUALIFIED_NO_NORMALIZATION_OR_CERTIFICATE_REUSE'},
 'native':{'candidate_sha256':sha(candidate),'bytes':len(candidate),'records':len(b['events']),
 'before_records':proof['prefix_completed_records'],'previous_deposits':proof['previous_deposits'],
 'travel_records':3,'head_parts':6,'cells':proof['cells'],'leaves':len(proof['leaves']),'work':proof['work']},
 'mandatory':{'checks':17,'pass_run':4,'unknown_not_run':13,'export':'BLOCK'}}
(raw/'provenance-final.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
