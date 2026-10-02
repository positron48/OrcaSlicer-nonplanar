from pathlib import Path
import gzip, hashlib, json
root=Path.cwd();raw=root/'build/nonplanar-evidence/B12-forming-contact'
parent=json.loads((root/'docs/nonplanar-dev/evidence/B12-final-deposition/source-manifest.json').read_text())
def sha(data):return hashlib.sha256(data).hexdigest()
files={};travel={};deposition={};old_material=[]
for path,archive in parent['raw_to_archive'].items():
 if '/ctest-native-evidence/' in path:current=raw/'final-candidate-evidence'/Path(path).name
 elif path.endswith('/ctest-job-evidence/native-lineage-report.json'):current=raw/'final-job-evidence/native-lineage-report.json'
 else:continue
 p=root/archive;data=gzip.decompress(p.read_bytes()) if p.suffix=='.gz' else p.read_bytes()
 pair={'parent':sha(data),'current':sha(current.read_bytes())}
 if current.name.startswith(('native-final-travel.','native-final-deposition.')) and current.name.endswith('.material.json'):
  old_material.append(json.loads(data));continue
 target=travel if current.name.startswith('native-final-travel.') else deposition if current.name.startswith('native-final-deposition.') else files
 target[current.name]=pair
assert len(files)==19 and len(travel)==4 and len(deposition)==4 and len(old_material)==2
differences=[name for name,pair in {**files,**travel,**deposition}.items() if pair['parent']!=pair['current']]
assert not differences,differences
new=raw/'final-candidate-evidence';focused=raw/'focus-final-native-evidence';inputs={}
for suffix in ['candidate.txt','rate.json','query.json','contact.json','material.json','proof.json','refusals.json']:
 name='native-final-forming-contact.'+suffix
 inputs[name]={'focus':sha((focused/name).read_bytes()),'ctest':sha((new/name).read_bytes())}
assert all(v['focus']==v['ctest'] for k,v in inputs.items() if not k.endswith('material.json'))
materials=[json.loads((focused/'native-final-forming-contact.material.json').read_text())]
materials += [json.loads((new/('native-final-'+stem+'.material.json')).read_text()) for stem in ['forming-contact','travel','deposition']]
materials += old_material
assert all(a['events']==materials[0]['events'] for a in materials)
policies=[{k:v for k,v in a['policy'].items() if k!='source_fingerprint'} for a in materials]
assert all(p==policies[0] for p in policies)
proof=json.loads((new/'native-final-forming-contact.proof.json').read_text());query=json.loads((new/'native-final-forming-contact.query.json').read_text())
assert proof['prefix_completed_records']==2198 and proof['previous_deposits']==2072
assert query['first_record']==2198 and query['record_count']==4
assert proof['cells']==28 and len(proof['leaves'])==28 and proof['export_allowed'] is False
assert sum(l['forming_contact'] for l in proof['leaves'])==4
candidate=(new/'native-final-forming-contact.candidate.txt').read_bytes();assert sha(candidate)=='b8eb15681127c1a8db57ce0aa257326763f16832b82fdafd13ed091c4bd98ef8'
for stem in ['travel','deposition']:assert candidate==(new/('native-final-'+stem+'.candidate.txt')).read_bytes()
report=json.loads((raw/'final-job-evidence/native-departure-report.json').read_text());assert report['candidate_bytes'].encode()==candidate
checks=json.loads(report['canonical'])['validation']['checks'];assert len(checks)==17
assert sum(c['status']=='PASS' and c['execution']=='RUN' for c in checks)==4
assert sum(c['status']=='UNKNOWN' and c['execution']=='NOT_RUN' for c in checks)==13
refusals=json.loads((new/'native-final-forming-contact.refusals.json').read_text())
assert refusals['contour']['status']=='UNKNOWN' and refusals['contour']['record_count']==108
assert refusals['hatch']['status']=='FAIL' and refusals['hatch']['record_count']==53
assert refusals['hatch']['witness']['progress']==[0,0] and refusals['hatch']['witness']['material_event']==2056
result={'status':'PASS_EXACT_OLD_FILES_BYTES_SCENE_POLICY_LEAVES_AND_NATIVE_CONTACT_REFUSALS','original_files':files,'old_travel_exact':travel,'old_deposition_exact':deposition,
 'differences':differences,'new_inputs':inputs,'actual_event_rows':'EXACT',
 'material_source_fingerprints':{'graphs':[m['policy']['source_fingerprint'] for m in materials],
 'scope':'COMPLETE_SOURCE_PROVENANCE_UNQUALIFIED_SEPARATE_OWNERS_NO_NORMALIZATION_OR_CERTIFICATE_REUSE'},
 'native':{'candidate_sha256':sha(candidate),'bytes':len(candidate),'records':len(materials[1]['events']),
 'before_records':proof['prefix_completed_records'],'previous_deposits':proof['previous_deposits'],'deposit_records':4,'head_parts':6,
 'cells':proof['cells'],'leaves':len(proof['leaves']),'contact_leaves':4,'work':proof['work'],'refusals':refusals},
 'mandatory':{'checks':17,'pass_run':4,'unknown_not_run':13,'export':'BLOCK'}}
(raw/'provenance-final.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'status':result['status'],'original_files':len(files),'old_travel_exact':len(travel),'old_deposition_exact':len(deposition),'native':result['native'],'mandatory':result['mandatory']},indent=2))
