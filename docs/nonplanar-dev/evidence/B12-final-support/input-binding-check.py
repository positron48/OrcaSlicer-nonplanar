from pathlib import Path
import gzip,hashlib,json
root=Path.cwd();raw=root/'build/nonplanar-evidence/B12-final-support';current=raw/'native-final'
manifest=root/'docs/nonplanar-dev/evidence/B12-final-joined/source-manifest.json';parent=json.loads(manifest.read_text())
parents={}
def old_bytes(name):
 key='build/nonplanar-evidence/B12-final-joined/native-final/'+name
 p=root/parent['raw_to_archive'][key];parents[str(p.relative_to(root))]=hashlib.sha256(p.read_bytes()).hexdigest()
 return gzip.open(p,'rb').read() if p.suffix=='.gz' else p.read_bytes()
old=json.loads(old_bytes('native-material.json'));new=json.loads((current/'native-material.json').read_text())
identity={name:old_bytes(name)==(current/name).read_bytes() for name in ['native-full-stop.candidate.txt','native-rate-policy.json','native-material.json']}
diff={k:{'parent':old['policy'][k],'current':v} for k,v in new['policy'].items() if old['policy'][k]!=v}
assert old['events']==new['events'] and not diff and all(identity.values())
binding={'records':len(new['events']),'depositions':sum(e['kind']=='deposit' for e in new['events']),'travel':sum(e['kind']=='travel' for e in new['events']),
'actual_prefix':{'completed_records':2217,'current_progress':.5},'parent_current_identical':identity,'parent_rows_identical':True,'parent_material_policy_differences':diff,
'parent':str(manifest.relative_to(root)),'parent_archive_sha256':parents,'sha256':{str(p.relative_to(raw)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(current.iterdir()) if p.is_file()},
'query':json.loads((current/'native-support-query.json').read_text())}
(raw/'native-support-input-binding.json').write_text(json.dumps(binding,indent=2)+'\n');print(json.dumps({'records':binding['records'],'depositions':binding['depositions'],'all_parent_inputs_identical':all(identity.values())}))
