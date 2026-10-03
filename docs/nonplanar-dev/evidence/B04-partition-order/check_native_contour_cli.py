from pathlib import Path
import hashlib,json,subprocess,time
root=Path.cwd();raw=root/'build/nonplanar-evidence/B04-partition-order';fixture=raw/'final-candidate-evidence';out=raw/'native-contour-cli-final';out.mkdir()
files=[fixture/('native-final-polyline-contact.'+s) for s in ['rate.json','material.json','contour-query.json','contact.json','candidate.txt']]
before={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
cmd=[str(root/'build/arm64/src/nonplanar_verify/Release/nonplanar_rate_audit'),'--linear-forming-contact-geometry-only',*map(str,files)]
t=time.monotonic();run=subprocess.run(cmd,capture_output=True,timeout=5);elapsed=time.monotonic()-t
(out/'stdout.json').write_bytes(run.stdout);(out/'stderr.txt').write_bytes(run.stderr)
report=json.loads(run.stdout);record={'command':cmd,'exit_code':run.returncode,'elapsed_seconds':elapsed,'report':report,'input_sha256':before}
(out/'run.json').write_text(json.dumps(record,indent=2)+'\n')
assert run.returncode==2 and report['component_status']=='FAIL',report
assert report['job_status']=='UNKNOWN' and report['export_allowed'] is False and report['leaves']==0
assert report['witness']['record']==2094 and report['witness']['material_event']==2077 and report['witness']['progress']==[1,1]
assert report['unproved_cell']['record']==2093 and report['cells']==461
assert report['work']<=2000000 and report['query']['record_count']==108
assert before=={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
print(json.dumps({'status':'PASS_EXPECTED_NATIVE_OLD_MATERIAL_FAIL_WITH_UNPROVED_EARLIER_CELL','underlying_exit_code':run.returncode,'CLI_work':report['work'],'cells':report['cells'],'wall_seconds':elapsed,'absolute_root_deadline_ms':1000,'job_export':'BLOCK'}))
