from pathlib import Path
from collections import Counter
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();stage='B06-native-hatch-precision'
raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
files=sorted([p for p in subprocess.check_output(['git','diff','--name-only'],text=True).splitlines() if not p.startswith('docs/')] + ['scripts/nonplanar/native_hatch_precision_oracle.py'])
assert files and all(not p.startswith('docs/') for p in files),files
assert len(files)==4 and len(set(files))==4,files
parent=json.loads((root/'docs/nonplanar-dev/evidence/B07-infill-density/source-manifest.json').read_text())
docs=['docs/nonplanar-dev/'+p for p in ['B06-native-hatch-precision.md','B06-native-hatch-precision-review.md','adr-0103-native-hatch-producer-precision.md','README.md','next-tasks.md','gate-b-plan.md','status.json']]
dependencies=sorted((set(parent['source_sha256'])|set(parent['dependency_sha256'])|set(docs))-set(files))
binaries=list(parent['binary_sha256'])
entries=json.loads((raw/'oracle-commands.json').read_text())
qualified=['build-final','build-unchanged','focus-final','ctest-final','cli','baseline-capture-final','baseline-compare-final','source-audit','package','compiler','bundle-prepare','bundle-isolation-final','diff-check-final','provenance-final','oracles-final']+[r['name']+'-oracle' for r in entries]
for n in qualified:assert json.loads((raw/(n+'.txt.json')).read_text())['exit_code']==0,n
focus=E.parse(raw/'focus-final.xml').getroot().find('testsuite')
focus_cases=focus.findall('testcase');assert focus_cases and all(not focus.get(k) or int(focus.get(k))==0 for k in ['errors','failures','skipped'])
discovered=json.loads((raw/'ctest-final/discovery.json').read_text())['tests']
ctest=E.parse(raw/'ctest-final/results.xml').getroot();executed=ctest.findall('testcase')
assert discovered and len(discovered)==len(executed)==int(ctest.get('tests'))
assert all(int(ctest.get(k,0))==0 for k in ['failures','disabled','skipped'])
for c in executed:assert c.get('status')=='run' and all(c.find(t) is None for t in ('failure','error','skipped'))
assert 'Building CXX' not in (raw/'build-unchanged.txt').read_text() and 'Linking' not in (raw/'build-unchanged.txt').read_text()
base=root/'build/nonplanar-evidence'/f'{stage}-final-off-zaa'
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(r['differential']=='PASS' for r in comparison)
assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/binaries[0])
assert sha(raw/'final-jobs/compiled-build-inputs.json')==sha(root/'build/arm64/src/libslic3r/nonplanar-build-inputs/Release/inventory.json')
provenance=json.loads((raw/'provenance.json').read_text());assert provenance['exact']==provenance['total']-1 and provenance['private_version_only_changes']==0
cli=json.loads((raw/'cli/manifest.json').read_text());assert len(cli['cases'])==17 and cli['binary_sha256']==sha(root/binaries[0])
isolation=json.loads((raw/'bundle-isolation.json').read_text());assert isolation['status']=='PASS'
assert isolation['manifest']['binary_sha256']==sha(root/binaries[0]) and isolation['manifest']['worker_sha256']==sha(root/binaries[-1])
corner=json.loads((raw/'corner-controller-oracle.txt').read_text());assert corner['status']=='PASS' and corner['mutation_refusals']==15
infill=json.loads((raw/'infill-replan-oracle.txt').read_text());assert infill['status']=='PASS' and infill['mutation_refusals']==10
precision=json.loads((raw/'native-hatch-precision-oracle.txt').read_text());assert precision['status']=='PASS' and precision['mutation_refusals']==12
assert len(entries)==23
for p in dependencies+binaries:assert (root/p).is_file(),p
commands=[];raw_hashes={};mapping={};dest.mkdir(parents=True)
for source,prefix in [(raw,Path()),(base,Path('baselines-final'))]:
 for path in sorted(source.rglob('*')):
  if not path.is_file() or path.name.startswith('archive-execution.'):continue
  rel=prefix/path.relative_to(source);name=str(path.relative_to(root));raw_hashes[name]=sha(path)
  if path.suffix=='.json':
   try:entry=json.loads(path.read_text())
   except (json.JSONDecodeError,UnicodeDecodeError):entry=None
   if isinstance(entry,dict) and entry.get('status')=='COMPLETED' and 'command' in entry:commands.append(dict(evidence=str(rel),**entry))
  compressed=path.suffix in ('.log','.txt','.gcode') or path.stat().st_size>250000
  if compressed:rel=Path(str(rel)+'.gz')
  target=dest/rel;assert not target.exists();target.parent.mkdir(parents=True,exist_ok=True)
  if compressed:
   with path.open('rb') as src,target.open('wb') as dst,gzip.GzipFile(filename='',mode='wb',fileobj=dst,mtime=0) as gz:shutil.copyfileobj(src,gz,1048576)
  else:shutil.copyfile(path,target)
  mapping[name]=str(target.relative_to(root))
(dest/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
m={'schema':1,'stage':stage,'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT',
 'commit_resolution':f'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/{stage}.md',
 'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},
 'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'final_build':'build-final.txt.json','unchanged_build':'build-unchanged.txt.json',
 'focus':{'cases':len(focus_cases),'assertions':int(focus.get('tests')),'evidence':['focus-final.xml'],'new_cases':2},
 'ctest_final':{'tests':len(executed),'failures':0,'skipped':0,'gate_elapsed_seconds':json.loads((raw/'ctest-final.txt.json').read_text())['elapsed_seconds'],'evidence':'ctest-final/results.xml'},
 'ctest_executables':dict(Counter(Path(t['command'][0]).stem for t in discovered)),
 'ctest_labels':dict(Counter('Nonplanar' if any(p['name']=='LABELS' and 'Nonplanar' in p['value'] for p in t['properties']) else 'other' for t in discovered)),
 'OFF_ZAA_final':comparison,'provenance':provenance,'compiled_inventory':provenance['compiled_inventory'],
 'qualified_scope':'NATIVE_PARENT_PRECISION_SELECTED_BEFORE_IMMUTABLE_STRIP_SPLIT_ONLY',
 'invariant':'ORIGINAL_ACTUAL_BODY_RESOURCES_AND_CONSUMER_PRECISION_RETAINED_MIN_ORIGINAL_HALF_CONSUMER_PRODUCER_NO_REFINE_OR_EXPORT',
 'versions':{'first_cap':6,'infill_replan':1,'request_native_plan_manifest':'UNCHANGED','all_private_versions':'UNCHANGED'},
 'retained_corner_controller_oracle':corner,'infill_oracle':infill,'native_hatch_precision_oracle':precision,'native_precision_cases':{name:{k:v for k,v in json.loads((raw/f'final-candidates/native-hatch-precision-{name}.json').read_text()).items() if k!='journal'} for name in ('parallel','transverse')},'identity_oracle_commands':len(entries),'default_candidate':provenance['default_candidate'],
 'mandatory_report':{'checks':17,'pass_run':4,'unknown_not_run':13},'guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS',
 'real_CLI_gate':cli,'isolated_bundle':isolation,'Linux':'NEW_HEAD_NOT_OBSERVED_BEFORE_PUSH','Windows':'NOT_RUN',
 'GUI':'NOT_RUN_BACKEND_CHANGE_PRIOR_GUI_HISTORICAL','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_KNOWN',
 'review':'AUTHOR_CRITICAL_REVIEW_INDEPENDENT_PENDING',
 'limits':'CONSTANT_NATIVE_ROOF_STRIP_VOLUME_NOT_COMPLETE_FIRST_CAP_OR_NATIVE_DENSITY_OR_FULL_JOB_OR_EXPORT',
 'remaining':'CERTIFIED_ACTUAL_ROOF_CHORD_POSITIVE_NATIVE_DENSITY_CAPTURED_CONTROLLER_CHILD_FINAL_BYTES_FULL_FILL_LAYERS_SEAMS_CONTACT_HEAD_ROUTES_ORDER_WHOLE_JOB_SOURCE_IMPORT_3MF_GUI_HARD_RESOURCES_ALL17_SOFTWARE_PHYSICAL_PUBLICATION',
 'test_environment':{'NPTOP_JOB_EVIDENCE_DIR':str(raw/'final-jobs'),'NPTOP_CANDIDATE_EVIDENCE_DIR':str(raw/'final-candidates')},'excluded_observations':json.loads((raw/'excluded-observations.json').read_text()),'qualified_commands':qualified,'commands':commands,
 'nonzero_commands':[c for c in commands if c['exit_code']!=0],'raw_sha256':raw_hashes,'raw_to_archive':mapping,
 'transport':'LOSSLESS_GZIP_LOG_TXT_GCODE_AND_LARGE_FILES_NO_FORCE_ADD'}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()}
(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for name,digest in raw_hashes.items():
 p=root/mapping[name];h=hashlib.sha256()
 with (gzip.open(p,'rb') if p.suffix=='.gz' else p.open('rb')) as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 assert h.hexdigest()==digest,name
print(json.dumps({'status':'PASS_FROZEN_LOSSLESS_EVIDENCE','files':len(m['archive_sha256'])+1,'sources':len(files),'dependencies':len(dependencies),'binaries':len(binaries),'raw_mappings':len(mapping),'commands':len(commands),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
