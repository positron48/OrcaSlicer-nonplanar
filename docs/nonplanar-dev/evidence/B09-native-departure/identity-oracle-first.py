from pathlib import Path
import gzip,hashlib,json,struct
root=Path.cwd();stage=root/'build/nonplanar-evidence/B09-native-departure';parent=root/'docs/nonplanar-dev/evidence/B09-exit-boundary'
manifest=json.loads((parent/'source-manifest.json').read_text())
def sha(data):return hashlib.sha256(data).hexdigest()
def parent_bytes(raw):
 p=root/manifest['raw_to_archive']['build/nonplanar-evidence/B09-exit-boundary/'+raw]
 return gzip.decompress(p.read_bytes()) if p.suffix=='.gz' else p.read_bytes()
old=json.loads((parent/'provenance-comparison.json').read_text())
files={k:{'parent':v['current'],'current':sha((stage/'final-native2'/k).read_bytes())} for k,v in old['original_files'].items()}
name='native-exit-witness.json';files[name]={'parent':sha(parent_bytes('final-ctest-native/'+name)),'current':sha((stage/'final-native2'/name).read_bytes())}
differences=[k for k,v in files.items() if v['parent']!=v['current']];assert not differences,differences
new=json.loads((stage/'final-job2/native-lineage-report.json').read_text());previous=json.loads(parent_bytes('final-job-fixtures/native-lineage-report.json'))
assert new['candidate_bytes']==previous['candidate_bytes'] and new['candidate_sha256']==previous['candidate_sha256']
checks=json.loads(new['canonical'])['validation']['checks'];assert len(checks)==17
assert sum(c['status']=='PASS' and c['execution']=='RUN' for c in checks)==4
assert sum(c['status']=='UNKNOWN' and c['execution']=='NOT_RUN' for c in checks)==13
departure=json.loads((stage/'final-native2/native-departure.json').read_text())
def journal(value):
 digest=sha(('nptop-material-ledger-v1\0'+value['context']).encode());context=json.loads(value['context'])
 assert context['record_count']==len(value['records'])
 for text in value['records']:digest=sha((digest+'\0'+text).encode())
 assert digest==value['sha256'];return context,[json.loads(x) for x in value['records']]
before,before_rows=journal(departure['before']);laid,laid_rows=journal(departure['laid']);planned,planned_rows=journal(departure['planned'])
assert departure['laid']['records'][:len(before_rows)]==departure['before']['records']
assert departure['planned']['records'][:len(laid_rows)]==departure['laid']['records']
assert len(planned_rows)==len(laid_rows)+3
assert laid['source']==planned['source']==before['source'] and laid['revision']==planned['revision']==before['revision']
assert laid['model']==planned['model'] and laid['model'][:5]==before['model'][:5]
binary=lambda s:struct.unpack('>d',bytes.fromhex(s))[0]
assert binary(laid['model'][5])>=binary(before['model'][5])
assert departure['source_records']==list(range(len(laid_rows)))+[len(laid_rows)]*3
assert len(set(r['motion'][0] for r in planned_rows))==len(planned_rows)
assert all(r['motion'][1]==i for i,r in enumerate(planned_rows))
assert all(r['motion'][7]==[0] and r['bead'] is None for r in planned_rows[-3:])
assert len(departure['legs'])==3 and len(departure['head'])==6 and departure['tip']['center'][2]==0
assert departure['tip']['opening_radius']==.2 and departure['tip']['outer_radius']==.5 and departure['required_margin']==.01
for leg in departure['legs']:
 assert leg['event_index']>=len(laid_rows)
 groups={(p[0],p[1]) for p in leg['leaves']}
 assert groups=={(c,i) for c in range(7) for i,r in enumerate(planned_rows[:leg['event_index']]) if r['bead'] is not None}
 assert all(0<=p[2]<p[3]<=1 for p in leg['leaves'])
candidate=(stage/'final-native2/native-departure.candidate.txt').read_bytes()
assert sha(candidate)==departure['candidate_sha256'] and len(candidate)==departure['candidate_bytes']
assert departure['candidate_rows']==len(planned_rows) and departure['selected_width_mm']==.28
assert departure['selected_target_volume'][1]<departure['original_wide_target_volume'][0]
assert departure['export']=='BLOCK' and departure['full_cap_fill']==departure['deposition_contact']==departure['final_byte_geometry']=='NOT_RUN'
result={'status':'PASS_DIAGNOSTIC_IDENTITY_ONLY_CPP_INDEPENDENT_PARTITION_IS_SEPARATE','original_files':files,'differences':differences,
 'native_lineage_candidate_sha256':new['candidate_sha256'],'mandatory':17,'pass_run':4,'unknown_not_run':13,'export':'BLOCK',
 'departure':{'parent_rows':len(before_rows),'laid_rows':len(laid_rows),'planned_rows':len(planned_rows),'candidate_sha256':departure['candidate_sha256'],
 'candidate_bytes':len(candidate),'evaluations':departure['work'],'legs':[dict(event=x['event_index'],cells=x['cells'],leaves=len(x['leaves']),evaluations=x['evaluations']) for x in departure['legs']]}}
(stage/'provenance-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
