from pathlib import Path
import gzip,hashlib,json
root=Path.cwd();raw=root/'build/nonplanar-evidence/B12-forming-polyline';current=raw/'final-candidate-evidence'
parent=json.loads((root/'docs/nonplanar-dev/evidence/B12-forming-contact/source-manifest.json').read_text())
def sha(data):return hashlib.sha256(data).hexdigest()
old={'original_files':{},'old_travel_exact':{},'old_deposition_exact':{},'old_contact_exact':{}};materials=[]
for path,archive in parent['raw_to_archive'].items():
 if '/final-candidate-evidence/' in path:p=current/Path(path).name
 elif path.endswith('/final-job-evidence/native-lineage-report.json'):p=raw/'final-job-evidence/native-lineage-report.json'
 else:continue
 f=root/archive;data=gzip.decompress(f.read_bytes()) if f.suffix=='.gz' else f.read_bytes()
 if p.name.endswith('.material.json') and p.name.startswith(('native-final-travel.','native-final-deposition.','native-final-forming-contact.')):
  materials.append(json.loads(data));continue
 key='old_travel_exact' if p.name.startswith('native-final-travel.') else 'old_deposition_exact' if p.name.startswith('native-final-deposition.') else 'old_contact_exact' if p.name.startswith('native-final-forming-contact.') else 'original_files'
 old[key][p.name]={'parent':sha(data),'current':sha(p.read_bytes())}
assert [len(old[k]) for k in old]==[19,4,4,6],[len(old[k]) for k in old]
differences=[p for files in old.values() for p,v in files.items() if v['parent']!=v['current']];assert not differences,differences
inputs={};focused=raw/'focus-qualified-native-evidence'
for suffix in ['candidate.txt','rate.json','query.json','contact.json','material.json','proof.json','contour-query.json','old-material-refusal.json']:
 name='native-final-polyline-contact.'+suffix
 inputs[name]={'focus':sha((focused/name).read_bytes()),'ctest':sha((current/name).read_bytes())}
assert all(v['focus']==v['ctest'] for k,v in inputs.items() if not k.endswith('material.json'))
for stem in ['travel','deposition','forming-contact','polyline-contact']:materials.append(json.loads((current/('native-final-'+stem+'.material.json')).read_text()))
materials.append(json.loads((focused/'native-final-polyline-contact.material.json').read_text()))
assert all(m['events']==materials[0]['events'] for m in materials)
policies=[{k:v for k,v in m['policy'].items() if k!='source_fingerprint'} for m in materials];assert all(p==policies[0] for p in policies)
proof=json.loads((current/'native-final-polyline-contact.proof.json').read_text());query=json.loads((current/'native-final-polyline-contact.query.json').read_text());contact=json.loads((current/'native-final-polyline-contact.contact.json').read_text())
assert query['first_record']==2198 and query['record_count']==4 and proof['prefix_completed_records']==2198 and proof['previous_deposits']==2072
assert contact['version']==2 and len(contact)==17 and contact['min_turn_cosine']==0
assert proof['export_allowed'] is False and proof['cells']==28 and len(proof['leaves'])==28
contact_leaves=[l for l in proof['leaves'] if l['forming_contact']];assert len(contact_leaves)==4
assert all(l['contact_records'] for l in contact_leaves)
candidate=(current/'native-final-polyline-contact.candidate.txt').read_bytes();assert sha(candidate)=='b8eb15681127c1a8db57ce0aa257326763f16832b82fdafd13ed091c4bd98ef8'
for stem in ['travel','deposition','forming-contact']:assert candidate==(current/('native-final-'+stem+'.candidate.txt')).read_bytes()
report=json.loads((raw/'final-job-evidence/native-departure-report.json').read_text());assert report['candidate_bytes'].encode()==candidate
checks=json.loads(report['canonical'])['validation']['checks'];assert len(checks)==17
assert sum(c['status']=='PASS' and c['execution']=='RUN' for c in checks)==4
assert sum(c['status']=='UNKNOWN' and c['execution']=='NOT_RUN' for c in checks)==13
refusal=json.loads((current/'native-final-polyline-contact.old-material-refusal.json').read_text())
assert refusal['status']=='FAIL' and refusal['job_status']=='UNKNOWN' and refusal['export_allowed'] is False and refusal['record_count']==108
assert refusal['witness']['record']==2094 and refusal['witness']['material_event']==2077 and refusal['witness']['progress']==[1,1]
assert refusal['unproved_cell']['record']==2093
native={'candidate_sha256':sha(candidate),'bytes':len(candidate),'records':len(materials[0]['events']),'before_records':proof['prefix_completed_records'],'previous_deposits':proof['previous_deposits'],'deposit_records':4,'head_parts':6,'cells':proof['cells'],'leaves':len(proof['leaves']),'contact_leaves':len(contact_leaves),'work':proof['work'],'refusal':refusal}
result={'status':'PASS_EXACT_OLD_NATIVE_TRAVEL_DEPOSIT_V1_CONTACT_INPUTS_PROOFS_AND_NEW_V2_FINAL_BYTES',**old,'differences':differences,'new_inputs':inputs,'actual_event_rows':'EXACT','material_source_fingerprints':{'graphs':[m['policy']['source_fingerprint'] for m in materials],'scope':'SEPARATE_COMPLETE_OWNER_GRAPHS_UNQUALIFIED_NO_NORMALIZATION_OR_CERTIFICATE_REUSE'},'native':native,'mandatory':{'checks':17,'pass_run':4,'unknown_not_run':13,'export':'BLOCK'}}
(raw/'provenance-final.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'status':result['status'],'exact_files':{k:len(v) for k,v in old.items()},'native':native,'mandatory':result['mandatory']},indent=2))
