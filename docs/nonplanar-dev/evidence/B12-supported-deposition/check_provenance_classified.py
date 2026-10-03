from pathlib import Path
import gzip,hashlib,json
root=Path.cwd();raw=root/'build/nonplanar-evidence/B12-supported-deposition';current=raw/'final-candidate-evidence'
parent=json.loads((root/'docs/nonplanar-dev/evidence/B12-forming-polyline/source-manifest.json').read_text())
def sha(data):return hashlib.sha256(data).hexdigest()
old={'original_files':{},'old_travel_exact':{},'old_deposition_exact':{},'old_contact_exact':{},'old_polyline_exact':{}};materials=[]
for path,archive in parent['raw_to_archive'].items():
 if '/final-candidate-evidence/' in path:p=current/Path(path).name
 elif path.endswith('/final-job-evidence/native-lineage-report.json'):p=raw/'final-job-evidence/native-lineage-report.json'
 else:continue
 f=root/archive;data=gzip.decompress(f.read_bytes()) if f.suffix=='.gz' else f.read_bytes()
 if p.name.endswith('.material.json') and p.name.startswith(('native-final-travel.','native-final-deposition.','native-final-forming-contact.','native-final-polyline-contact.')):
  materials.append(json.loads(data));continue
 key='old_travel_exact' if p.name.startswith('native-final-travel.') else 'old_deposition_exact' if p.name.startswith('native-final-deposition.') else 'old_contact_exact' if p.name.startswith('native-final-forming-contact.') else 'old_polyline_exact' if p.name.startswith('native-final-polyline-contact.') else 'original_files'
 old[key][p.name]={'parent':sha(data),'current':sha(p.read_bytes())}
assert [len(old[k]) for k in old]==[19,4,4,6,7],[len(old[k]) for k in old]
differences=[p for files in old.values() for p,v in files.items() if v['parent']!=v['current']]
# Classify observed differences explicitly; never normalize runtime fingerprints.
def parent_json(name):
 f=root/next(v for k,v in parent['raw_to_archive'].items() if '/final-candidate-evidence/' in k and Path(k).name==name)
 return json.loads(gzip.decompress(f.read_bytes()) if f.suffix=='.gz' else f.read_bytes())
classified={'status':'ALL_ORIGINAL_OUTPUTS_EXACT','unchanged_original_files':19,'changed_files':[]}
if differences:
 a=parent_json('native-partition-meshes.json');b=json.loads((current/'native-partition-meshes.json').read_text())
 assert set(a)==set(b) and all(a[k]==b[k] for k in a if k!='body')
 x,y=a['body'],b['body'];assert set(x)==set(y) and all(x[k]==y[k] for k in x if k not in ['vertices','indices'])
 assert len(x['vertices'])==len(y['vertices'])==22 and len(x['indices'])==len(y['indices'])==40
 positions={tuple(v):i for i,v in enumerate(y['vertices'])};assert len(positions)==22
 remap=[positions[tuple(v)] for v in x['vertices']];assert [(i,j) for i,j in enumerate(remap) if i!=j]==[(16,17),(17,16)]
 assert [[remap[i] for i in face] for face in x['indices']]==y['indices']
 def expand(value):
  if isinstance(value,dict):return {k:expand(v) for k,v in value.items()}
  if isinstance(value,list):return [expand(v) for v in value]
  if isinstance(value,str) and value.startswith(('{','[')):
   try:return expand(json.loads(value))
   except ValueError:pass
  return value
 def changed_paths(a,b,path=''):
  assert type(a)==type(b),path
  if isinstance(a,dict):
   assert set(a)==set(b),path
   return sum((changed_paths(a[k],b[k],path+'/'+k) for k in a),[])
  if isinstance(a,list):
   assert len(a)==len(b),path
   return sum((changed_paths(x,y,path+'/'+str(i)) for i,(x,y) in enumerate(zip(a,b))),[])
  return [] if a==b else [path]
 expected={
  'native-provenance.json':['/body/guarded_settings','/body/partition','/body_material/body','/body_material/material','/final_source_fingerprint','/guarded/input_fingerprint','/partition/body'],
  'native-exit-witness.json':['/context/source','/ledger_fingerprint'],
  'native-departure-material.json':['/policy/source_fingerprint'],
  'native-material.json':['/policy/source_fingerprint'],
  'native-departure.json':['/before/context/source','/before/sha256','/laid/context/source','/laid/sha256','/planned/context/source','/planned/sha256'],
  'native-job-report.json':['/canonical/manifest_sha256','/manifest/material_journal','/manifest/source_fingerprint','/manifest_sha256','/material_journal','/sha256','/source_fingerprint']}
 paths={}
 for name,allowed in expected.items():
  paths[name]=changed_paths(expand(parent_json(name)),expand(json.loads((current/name).read_text())))
  assert sorted(paths[name])==sorted(allowed),(name,paths[name])
 for p in [parent_json('native-provenance.json'),json.loads((current/'native-provenance.json').read_text())]:
  assert sha(p['body'].encode())==p['final_source_fingerprint']
  body=json.loads(p['body']);assert bytes.fromhex(body['partition']).decode()==sha(p['partition'].encode())
  assert bytes.fromhex(body['guarded_settings']).decode()==sha(p['guarded'].encode())
  assert bytes.fromhex(json.loads(p['body_material'])['body']).decode()==p['final_source_fingerprint']
 classified={'status':'STRICT_IDENTITY_DIFFERENCES_RETAINED_EXACT_DERIVED_BODY_REMAP_DIAGNOSIS_ONLY','changed_files':differences,'unchanged_original_files':len(old['original_files'])-len(differences),'body_vertex_remap':remap,'vertices':22,'oriented_triangles':40,'unchanged_subtrees':'ALL_OTHER_PARTITION_MESHES_PROPERTIES_SOURCE_EXECUTED_CONFIGS_PATHS_AND_NONDEPENDENCY_LEAVES_EXACT','changed_dependency_paths':paths,'scope':'CGAL_INTERNAL_ORDER_CROSS_PLATFORM_DETERMINISM_FULL_SOURCE_TO_PLAN_BINDING_UNQUALIFIED_NO_RUNTIME_NORMALIZATION_OR_CERTIFICATE_REUSE'}
inputs={};focused=raw/'focus-qualified-native-evidence'
for suffix in ['candidate.txt','rate.json','query.json','contact.json','material.json','proof.json','contour-query.json','old-material-refusal.json']:
 name='native-final-polyline-contact.'+suffix
 inputs[name]={'focus':sha((focused/name).read_bytes()),'ctest':sha((current/name).read_bytes())}
assert all(v['focus']==v['ctest'] for k,v in inputs.items() if not k.endswith('material.json'))
for stem in ['travel','deposition','forming-contact','polyline-contact']:materials.append(json.loads((current/('native-final-'+stem+'.material.json')).read_text()))
materials.append(json.loads((focused/'native-final-polyline-contact.material.json').read_text()))
materials.append(json.loads((current/'native-final-supported-deposition.material.json').read_text()))
materials.append(json.loads((focused/'native-final-supported-deposition.material.json').read_text()))
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
result={'status':'PASS_EXACT_PRIOR_PROOFS_AND_SUPPORTED_DEPOSITION_WITH_EXPLICIT_SOURCE_IDENTITY_DIAGNOSIS','source_drift':classified,**old,'differences':differences,'new_inputs':inputs,'actual_event_rows':'EXACT','material_source_fingerprints':{'graphs':[m['policy']['source_fingerprint'] for m in materials],'scope':'SEPARATE_COMPLETE_OWNER_GRAPHS_UNQUALIFIED_NO_NORMALIZATION_OR_CERTIFICATE_REUSE'},'native':native,'mandatory':{'checks':17,'pass_run':4,'unknown_not_run':13,'export':'BLOCK'}}
(raw/'provenance-classified.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'status':result['status'],'exact_files':{k:len(v) for k,v in old.items()},'native':native,'mandatory':result['mandatory']},indent=2))

new_inputs={}
for suffix in ['candidate.txt','rate.json','material.json','query.json','contact.json','proof.json','support.json','support-proof.json']:
 name='native-final-supported-deposition.'+suffix
 new_inputs[name]={'focus':sha((focused/name).read_bytes()),'ctest':sha((current/name).read_bytes())}
assert all(v['focus']==v['ctest'] for k,v in new_inputs.items() if not k.endswith('material.json'))
assert candidate==(current/'native-final-supported-deposition.candidate.txt').read_bytes()
combined=json.loads((current/'native-final-supported-deposition.support-proof.json').read_text())
assert combined['component_status']=='PASS' and combined['job_status']=='UNKNOWN' and combined['export_allowed'] is False
assert combined['work']==637876 and combined['cells']==48 and combined['first_record']==2198 and combined['record_count']==4
assert len(combined['support'])==1 and combined['support'][0]['underlying_completed_records']==2198
assert combined['support'][0]['cells']==20 and len(combined['support'][0]['leaves'])==4
result['supported']={'inputs':new_inputs,'proof':combined,'scope':'COMPLETE_DECLARED_BLOCK_AND_PRE_BLOCK_UNDERLYING_SUPPORT_ONLY'}
(raw/'provenance-classified.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'status':'PASS_SUPPORTED_NATIVE_EXACT_BYTES_AND_ALL_PUBLISHED_LEAVES','cells':combined['cells'],'work':combined['work']}))
