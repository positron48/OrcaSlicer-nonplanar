from pathlib import Path
import hashlib,json
raw=Path('build/nonplanar-evidence/B11-rounded-rates');old=Path('build/nonplanar-evidence/B14-later-controller/final-candidates')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
files=[]
for p in sorted(old.iterdir()):
 if not p.is_file():continue
 q=raw/'final-candidates'/p.name;assert q.is_file(),p.name
 files.append({'name':p.name,'exact':sha(p)==sha(q),'parent_sha256':sha(p),'current_sha256':sha(q)})
differences=[r for r in files if not r['exact']]
assert len(files)==51 and len(differences)==10
work_only=[]
def strip_work(v):
 if isinstance(v,dict):return {k:strip_work(x) for k,x in v.items() if k!='work'}
 if isinstance(v,list):return [strip_work(x) for x in v]
 return v
for row in differences:
 if row['name']=='native-job-report.json':continue
 a=json.loads((old/row['name']).read_text());b=json.loads((raw/'final-candidates'/row['name']).read_text())
 assert strip_work(a)==strip_work(b),row['name'];work_only.append(row['name'])
assert len(work_only)==9
old_job=json.loads((old/'native-job-report.json').read_text());new_job=json.loads((raw/'final-candidates/native-job-report.json').read_text())
for k in ('candidate_bytes','candidate_sha256','initial_position','material_journal','motion_policy','serializer_policy','source_fingerprint','source_revision'):assert old_job[k]==new_job[k],k
report=json.loads((raw/'final-jobs/native-source-reference-diagnostic.json').read_text())
validation=report['report']['validation'];checks=validation['checks']
assert validation['gcode_sha256']=='c330363d909d5907a52b151a7ca499d1ce72681cc0185edd6f9c312bef12686b'
assert len(report['replay'])==2098 and len(checks)==17
assert sum(c['status']=='PASS' and c['execution']=='RUN' for c in checks)==4
assert sum(c['status']=='UNKNOWN' and c['execution']=='NOT_RUN' for c in checks)==13
inventory=raw/'final-jobs/compiled-build-inputs.json';p=json.loads(inventory.read_text())
assert sha(inventory)==sha(Path('build/arm64/src/libslic3r/nonplanar-build-inputs/Release/inventory.json'))
watchdog=json.loads((raw/'final-jobs/native-watchdog-observations.json').read_text())
for r in watchdog:assert r['alive_before_callback'] and r['alive_inside_blocked_callback']==(r['scenario']=='healthy') and not r['alive_after_return'] and r['diagnostic_empty']
result={'total':len(files),'exact':len(files)-len(differences),'differences':differences,'work_only_changes':work_only,'software_bound_report_changes':1,'files':files,'compiled_inventory':{'files':len(p['files']),'bytes':inventory.stat().st_size,'sha256':sha(inventory)},'default_candidate':{'records':2098,'sha256':validation['gcode_sha256']},'mandatory':{'checks':17,'pass_run':4,'unknown_not_run':13},'watchdog_observations':watchdog,'export':'BLOCK','full_B01_B15':'IN_PROGRESS'}
(raw/'provenance.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k not in ('files','watchdog_observations')}))
