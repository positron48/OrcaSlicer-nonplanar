from pathlib import Path
import gzip, hashlib, json
root=Path.cwd();raw=root/'build/nonplanar-evidence/B12-final-deposition'
parent=json.loads((root/'docs/nonplanar-dev/evidence/B12-final-travel/source-manifest.json').read_text())
def sha(data):return hashlib.sha256(data).hexdigest()
files={};travel={};old_material=None
for path,archive in parent['raw_to_archive'].items():
 if '/ctest-final2-native-evidence/' in path:current=raw/'ctest-native-evidence'/Path(path).name
 elif path.endswith('/ctest-final2-job-evidence/native-lineage-report.json'):current=raw/'ctest-job-evidence/native-lineage-report.json'
 else:continue
 p=root/archive;data=gzip.decompress(p.read_bytes()) if p.suffix=='.gz' else p.read_bytes()
 pair={'parent':sha(data),'current':sha(current.read_bytes())}
 if current.name=='native-final-travel.material.json':old_material=json.loads(data);continue
 (travel if current.name.startswith('native-final-travel.') else files)[current.name]=pair
assert len(files)==19 and len(travel)==4
differences=[name for name,pair in {**files,**travel}.items() if pair['parent']!=pair['current']]
assert not differences,differences
new=raw/'ctest-native-evidence';focused=raw/'focus5-native-evidence';inputs={}
for suffix in ['candidate.txt','rate.json','query.json','material.json','proof.json']:
 name='native-final-deposition.'+suffix
 inputs[name]={'focus':sha((focused/name).read_bytes()),'ctest':sha((new/name).read_bytes())}
assert all(v['focus']==v['ctest'] for k,v in inputs.items() if not k.endswith('material.json'))
materials=[json.loads((focused/'native-final-deposition.material.json').read_text()),json.loads((new/'native-final-deposition.material.json').read_text()),
 json.loads((new/'native-final-travel.material.json').read_text()),old_material]
assert all(a['events']==materials[0]['events'] for a in materials)
policies=[{k:v for k,v in a['policy'].items() if k!='source_fingerprint'} for a in materials]
assert all(p==policies[0] for p in policies)
proof=json.loads((new/'native-final-deposition.proof.json').read_text());query=json.loads((new/'native-final-deposition.query.json').read_text())
assert proof['prefix_completed_records']==2198 and proof['previous_deposits']==2072
assert query['first_record']==2198 and query['record_count']==4
assert proof['cells']==300 and len(proof['leaves'])==164 and proof['work']==748634 and proof['export_allowed'] is False
candidate=(new/'native-final-deposition.candidate.txt').read_bytes();assert sha(candidate)=='b8eb15681127c1a8db57ce0aa257326763f16832b82fdafd13ed091c4bd98ef8'
assert candidate==(new/'native-final-travel.candidate.txt').read_bytes()
report=json.loads((raw/'ctest-job-evidence/native-departure-report.json').read_text());assert report['candidate_bytes'].encode()==candidate
checks=json.loads(report['canonical'])['validation']['checks'];assert len(checks)==17
assert sum(c['status']=='PASS' and c['execution']=='RUN' for c in checks)==4
assert sum(c['status']=='UNKNOWN' and c['execution']=='NOT_RUN' for c in checks)==13
result={'status':'PASS_EXACT_OLD_FILES_AND_ACTUAL_NEW_BYTES_SCENE_POLICY_LEAVES_ONLY','original_files':files,'old_travel_exact':travel,
 'differences':differences,'new_inputs':inputs,'actual_event_rows':'EXACT',
 'material_source_fingerprints':{'focus':materials[0]['policy']['source_fingerprint'],'ctest':materials[1]['policy']['source_fingerprint'],
 'travel':materials[2]['policy']['source_fingerprint'],'parent_travel':materials[3]['policy']['source_fingerprint'],
 'scope':'COMPLETE_SOURCE_PROVENANCE_UNQUALIFIED_NO_NORMALIZATION_OR_CERTIFICATE_REUSE'},
 'native':{'candidate_sha256':sha(candidate),'bytes':len(candidate),'records':len(materials[1]['events']),
 'before_records':proof['prefix_completed_records'],'previous_deposits':proof['previous_deposits'],'deposit_records':4,'head_parts':6,
 'cells':proof['cells'],'leaves':len(proof['leaves']),'work':proof['work']},
 'mandatory':{'checks':17,'pass_run':4,'unknown_not_run':13,'export':'BLOCK'}}
(raw/'provenance-final.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
