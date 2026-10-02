import hashlib,json,pathlib
r=pathlib.Path('build/nonplanar-evidence/B13-final-report');parent=pathlib.Path('build/nonplanar-evidence/B13-job-context/native-final4')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
a=json.loads((r/'native-root2/native-partition-meshes.json').read_text());b=json.loads((r/'native-final/native-partition-meshes.json').read_text())
source=a['body'];target=b['body'];assert len(source['vertices'])==len(target['vertices'])==22
positions={tuple(v):i for i,v in enumerate(target['vertices'])};assert len(positions)==22
remap=[positions[tuple(v)] for v in source['vertices']]
assert [(i,j) for i,j in enumerate(remap) if i!=j]==[(16,17),(17,16)]
assert [[remap[i] for i in face] for face in source['indices']]==target['indices']
assert len(source['indices'])==40 and source['properties']==target['properties']
assert all(a[name]==b[name] for name in ['cap','original','reservation'])
contexts={}
for name in ['native-root','native-test-cwd','native-root2','native-test-cwd2','native-final','ctest-native','native-final2','ctest-native2']:
 folder=r/name;record=json.loads((folder/'native-provenance.json').read_text());contexts[name]={'source_fingerprint':record['final_source_fingerprint'],
  'parent_file_identity':{p.name:p.read_bytes()==(folder/p.name).read_bytes() for p in parent.iterdir()},
  'sha256':{p.name:sha(p) for p in folder.iterdir() if p.is_file()}}
 for filename,same in contexts[name]['parent_file_identity'].items():assert same or filename=='native-material.json'
 if not contexts[name]['parent_file_identity']['native-material.json']:
  old=json.loads((parent/'native-material.json').read_text());new=json.loads((folder/'native-material.json').read_text());old['policy'].pop('source_fingerprint');new['policy'].pop('source_fingerprint');assert old==new
pa=json.loads((r/'native-root2/native-provenance.json').read_text());pb=json.loads((r/'native-final/native-provenance.json').read_text())
for key in ['input','executed_full','executed_print','executed_object']:assert pa[key]==pb[key]
partition_a=json.loads(pa['partition']);partition_b=json.loads(pb['partition']);assert partition_a.pop('body')!=partition_b.pop('body');assert partition_a==partition_b
body_a=json.loads(pa['body']);body_b=json.loads(pb['body']);assert body_a.pop('partition')!=body_b.pop('partition');assert body_a.pop('guarded_settings')!=body_b.pop('guarded_settings');assert body_a==body_b
guarded_a=json.loads(pa['guarded']);guarded_b=json.loads(pb['guarded']);assert guarded_a.pop('input_fingerprint')!=guarded_b.pop('input_fingerprint');assert guarded_a==guarded_b
record={'status':'DIFFERENCE_LOCALIZED_STRICT_IDENTITY_REFUSAL_RETAINED','scope':'EXACT_INDEXED_DERIVED_BODY_REPRESENTATION_DIAGNOSTIC_ONLY',
 'body_vertex_remap':remap,'vertices':22,'oriented_triangles':40,'actual_vertices_and_oriented_triangles_same_after_explicit_remap':True,
 'cap_original_reservation_exact':True,'source_and_executed_configs_exact':True,'earlier_strict_file_refusals_retained':True,
 'cause_observed':'DERIVED_BODY_VERTEX16_17_EXCHANGED_WITH_CORRESPONDING_ORIENTED_FACE_INDICES_IN_NATIVE_CGAL_PARTITION_OUTPUT',
 'not_qualified':'CGAL_INTERNAL_ORDER_OR_CROSS_PLATFORM_DETERMINISM_AND_FULL_MODEL_TO_PLAN_JOB_BINDING_NOT_PROVEN; NO_RUNTIME_HASH_NORMALIZATION',
 'contexts':contexts}
(r/'native-provenance-difference-final2.json').write_text(json.dumps(record,indent=2)+'\n');print(json.dumps({k:v for k,v in record.items() if k!='contexts'}))
