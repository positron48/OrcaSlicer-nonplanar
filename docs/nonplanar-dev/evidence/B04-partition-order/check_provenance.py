from pathlib import Path
import hashlib,json
root=Path.cwd();raw=root/'build/nonplanar-evidence/B04-partition-order';current=raw/'final-candidate-evidence';parent=root/'build/nonplanar-evidence/B12-supported-deposition/final-candidate-evidence'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def oriented_geometry(mesh):
 points=[tuple(v) for v in mesh['vertices']]
 triangles=[]
 for f in mesh['indices']:
  p=tuple(points[i] for i in f)
  triangles.append(min(p,p[1:]+p[:1],p[2:]+p[:2]))
 return sorted(points),sorted(triangles)
old=json.loads((parent/'native-partition-meshes.json').read_text());new=json.loads((current/'native-partition-meshes.json').read_text())
assert set(old)==set(new)
geometry={}
for name in old:
 a,b=old[name],new[name];assert set(a)==set(b)
 assert all(a[k]==b[k] for k in a if k not in ['vertices','indices']),name
 assert oriented_geometry(a)==oriented_geometry(b),name
 if name in ['original','reservation']:assert a==b,name
 geometry[name]={'old_sha256':sha(parent/'native-partition-meshes.json'),'new_sha256':sha(current/'native-partition-meshes.json'),'vertices':len(b['vertices']),'oriented_triangles':len(b['indices']),'same_oriented_coordinate_multiset':True}
old_files={}
for p in sorted(parent.iterdir()):
 q=current/p.name;assert q.is_file(),p.name
 a,b=p.read_bytes(),q.read_bytes();old_files[p.name]={'old_sha256':sha(p),'new_sha256':sha(q),'exact':a==b}
 if p.name.endswith('.candidate.txt'):assert a==b,p.name
 if p.name.startswith('native-final-') and not p.name.endswith('.material.json'):assert a==b,p.name
 if p.name.endswith('.material.json'):
  x,y=json.loads(a),json.loads(b);assert x['events']==y['events']
  assert {k:v for k,v in x['policy'].items() if k!='source_fingerprint'}=={k:v for k,v in y['policy'].items() if k!='source_fingerprint'}
repeat={}
focused=raw/'focus1-native-evidence'
for p in sorted(focused.iterdir()):
 q=current/p.name;assert q.is_file() and p.read_bytes()==q.read_bytes(),p.name
 repeat[p.name]={'focus_sha256':sha(p),'ctest_sha256':sha(q),'all_bytes_exact':True}
# The earlier-source and independent full-lineage tests run in separate native
# processes. Their actual indexed meshes and dependent fingerprints must agree.
provenance=json.loads((current/'native-provenance.json').read_text());body=json.loads(provenance['body'])
assert hashlib.sha256(provenance['body'].encode()).hexdigest()==provenance['final_source_fingerprint']
lineage=json.loads((raw/'final-job-evidence/native-lineage-report.json').read_text())
checks=json.loads(json.loads((raw/'final-job-evidence/native-departure-report.json').read_text())['canonical'])['validation']['checks']
assert len(checks)==17 and sum(c['status']=='PASS' and c['execution']=='RUN' for c in checks)==4
assert sum(c['status']=='UNKNOWN' and c['execution']=='NOT_RUN' for c in checks)==13
candidate=current/'native-final-supported-deposition.candidate.txt'
assert sha(candidate)=='b8eb15681127c1a8db57ce0aa257326763f16832b82fdafd13ed091c4bd98ef8'
result={'status':'PASS_ACTUAL_DERIVED_ORDER_STABLE_SEPARATE_PROCESSES_AND_ORIENTED_GEOMETRY_PRESERVED','geometry':geometry,'old_files':old_files,'all_focus_ctest_outputs_exact':repeat,'source_fingerprint':provenance['final_source_fingerprint'],'candidate_sha256':sha(candidate),'bytes':candidate.stat().st_size,'scope':'ACTUAL_RUNTIME_DERIVED_REPRESENTATION_FIXED_NO_HASH_NORMALIZATION_CERTIFICATE_REUSE_OR_UNIVERSAL_TRIANGULATION_DETERMINISM','mandatory':{'checks':17,'pass_run':4,'unknown_not_run':13,'export':'BLOCK'}}
(raw/'provenance.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'status':result['status'],'native_outputs_compared':len(old_files),'separate_process_outputs_exact':len(repeat),'geometry':{k:[v['vertices'],v['oriented_triangles']] for k,v in geometry.items()},'old_differences':[k for k,v in old_files.items() if not v['exact']],'candidate_sha256':sha(candidate),'source_fingerprint':result['source_fingerprint']}))
