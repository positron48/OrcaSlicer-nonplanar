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
 files[name]={'parent':sha(archived(path)),'current':sha((raw/'final-native-evidence'/name).read_bytes())}
old='build/nonplanar-evidence/B09-native-departure/final-job2/native-lineage-report.json'
files['native-lineage-report.json']={'parent':sha(archived(old)),'current':sha((raw/'final-job-evidence/native-lineage-report.json').read_bytes())}
differences=[name for name,pair in files.items() if pair['parent']!=pair['current']]
assert not differences,differences
record=json.loads((raw/'final-job-evidence/native-departure-report.json').read_text())
focus=json.loads((raw/'focus-evidence/native-departure-report.json').read_text())
assert record['candidate_bytes']==focus['candidate_bytes'] and record['candidate_sha256']==focus['candidate_sha256']
for name in ['body','before','assembled','routed','planned']:
 a=record['native'][name];b=focus['native'][name]
 assert a['records']==b['records'],name
 ca=json.loads(a['context']);cb=json.loads(b['context'])
 assert {k:v for k,v in ca.items() if k!='source'}=={k:v for k,v in cb.items() if k!='source'},name
for key in ['job_canonical','job_fingerprint','motion_policy','serializer_policy']:
 assert record[key]==focus[key],key
for key in ['body_canonical','hatch_canonical','departure_canonical']:
 a=json.loads(record['native'][key]);b=json.loads(focus['native'][key])
 allowed={'body_canonical':{'body','body_journal','body_material'},'hatch_canonical':{'body_lineage'},
          'departure_canonical':{'before_journal','laid_journal','routed_journal'}}[key]
 assert {k:v for k,v in a.items() if k not in allowed}=={k:v for k,v in b.items() if k not in allowed},key
assert record['source_fingerprint']!=focus['source_fingerprint']
identity={'status':'DIFFERENT_DERIVED_BODY_IDENTITY_UNQUALIFIED_NO_REUSE_NO_RUNTIME_NORMALIZATION',
 'focus_source_fingerprint':focus['source_fingerprint'],'final_source_fingerprint':record['source_fingerprint'],
 'focus_report_sha256':focus['sha256'],'final_report_sha256':record['sha256'],
 'all_actual_rows_and_candidate_bytes':'EXACT','job_request_hatch_geometry_scene_policy_origin_map':'EXACT',
 'derived_body_fingerprint_cause':'NOT_QUALIFIED_THIS_STAGE'}
n=record['native'];departure=json.loads(n['departure_canonical']);plan=json.loads(n['canonical']);manifest=json.loads(record['manifest'])
assert plan['schema']==2 and manifest['schema']==3
checks=json.loads(record['canonical'])['validation']['checks'];assert len(checks)==17
assert sum(c['status']=='PASS' and c['execution']=='RUN' for c in checks)==4
assert sum(c['status']=='UNKNOWN' and c['execution']=='NOT_RUN' for c in checks)==13
result={'status':'PASS_EXACT_ORIGINAL_OUTPUT_AND_NEW_ROW_BYTE_COMPARISON_ONLY','new_candidate_dependency_identity':identity,'original_files':files,'differences':differences,
 'native_departure':{'journals':{k:len(n[k]['records']) for k in ['body','before','assembled','routed','planned']},
 'candidate_bytes':len(record['candidate_bytes'].encode()),'candidate_sha256':record['candidate_sha256'],
 'departure_sha256':n['departure_sha256'],'native_plan_sha256':n['sha256'],'manifest_sha256':record['manifest_sha256'],
 'report_sha256':record['sha256'],'legs':departure['legs'],'mandatory':17,'pass_run':4,'unknown_not_run':13,'export':'BLOCK'}}
(raw/'provenance-final2.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
