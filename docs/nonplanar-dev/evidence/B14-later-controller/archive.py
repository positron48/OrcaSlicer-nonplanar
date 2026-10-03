from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();raw=root/'build/nonplanar-evidence/B14-later-controller';dest=root/'docs/nonplanar-dev/evidence/B14-later-controller'
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==19,files
for p in files:assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==sha(root/p),p
parent=json.loads((root/'docs/nonplanar-dev/evidence/B07-later-sequence/source-manifest.json').read_text())
docs=['docs/nonplanar-dev/'+p for p in ['B14-later-controller.md','B14-later-controller-review.md','adr-0097-captured-later-controller-lineage.md','README.md','next-tasks.md','gate-b-plan.md','status.json']]
dependencies=sorted((set(parent['source_sha256'])|set(parent['dependency_sha256'])|set(docs))-set(files))
binaries=list(parent['binary_sha256']);assert len(binaries)==5
entries=json.loads((raw/'oracle-commands.json').read_text());assert len(entries)==17
qualified=['build-final2','build-unchanged','focus-final','ctest-final','cli','baseline-capture-final','baseline-compare-final','source-audit','package','compiler','bundle-prepare','bundle-isolation','diff-check','provenance','oracles','later-focus-oracle2']+[r['name']+'-oracle' for r in entries]
for n in qualified:assert json.loads((raw/(n+'.txt.json')).read_text())['exit_code']==0,n
focus=E.parse(raw/'focus-final.xml').getroot().find('testsuite');assert len(focus.findall('testcase'))==5 and focus.get('tests')=='2030' and focus.get('failures')=='0'
ctest=E.parse(raw/'ctest-final/results.xml').getroot();assert ctest.get('tests')=='484' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest-final/discovery.json').read_text())['tests'])==484
for c in ctest.findall('testcase'):assert c.get('status')=='run' and all(c.find(t) is None for t in ('failure','error','skipped'))
assert 'Building CXX' not in (raw/'build-unchanged.txt').read_text() and 'Linking' not in (raw/'build-unchanged.txt').read_text()
base=root/'build/nonplanar-evidence/B14-later-controller-final-off-zaa'
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(r['differential']=='PASS' for r in comparison)
assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/binaries[0])
assert sha(raw/'final-jobs/compiled-build-inputs.json')==sha(root/'build/arm64/src/libslic3r/nonplanar-build-inputs/Release/inventory.json')
provenance=json.loads((raw/'provenance.json').read_text());assert provenance['exact']==50 and provenance['total']==51
cli=json.loads((raw/'cli/manifest.json').read_text());assert len(cli['cases'])==13 and cli['binary_sha256']==sha(root/binaries[0])
isolation=json.loads((raw/'bundle-isolation.json').read_text());assert isolation['status']=='PASS'
assert isolation['manifest']['binary_sha256']==sha(root/binaries[0]) and isolation['manifest']['worker_sha256']==sha(root/binaries[-1])
observations=provenance['watchdog_observations'];assert len(observations)==6
assert [r['scenario'] for r in observations]==['deadline','memory','output','progress','stale','healthy']
for r in observations:assert r['alive_before_callback'] and r['alive_inside_blocked_callback']==(r['scenario']=='healthy') and not r['alive_after_return'] and r['diagnostic_empty']
assert json.loads((raw/'binding-red.txt.json').read_text())['exit_code']==42 and 'omitted.snapshot' in (raw/'binding-red.xml').read_text()
later=json.loads((raw/'later-oracle.txt').read_text());assert later['status']=='PASS' and later['fine_records']==1756 and later['default_records']==1760 and later['mutation_refusals']==8
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
m={'schema':1,'stage':'B14-later-controller','recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT',
 'commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B14-later-controller.md',
 'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},
 'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'final_build':'build-final2.txt.json','unchanged_build':'build-unchanged.txt.json',
 'focus':{'cases':5,'assertions':2030,'evidence':'focus-final.xml'},
 'ctest_final':{'tests':484,'failures':0,'skipped':0,'gate_elapsed_seconds':json.loads((raw/'ctest-final.txt.json').read_text())['elapsed_seconds'],'evidence':'ctest-final/results.xml'},
 'ctest_executables':{'nonplanar_tests':242,'fff_print_tests':242},'ctest_labels':{'Nonplanar':445,'other':39},
 'OFF_ZAA_final':comparison,'provenance':provenance,'compiled_inventory':provenance['compiled_inventory'],
 'qualified_scope':'CAPTURED_ORDERED_LOCAL_LATER_CONTROLLER_NATIVE_LINEAGE_AND_ACTUAL_FINAL_BYTE_REPLAY_ONLY',
 'invariant':'EXACT_CAPTURED_REQUEST_PROGRAM_ALL_PRIVATE_PREFIX_OWNERS_FINAL_LEDGER_MOTION_AND_BYTES_NO_OMISSION_OR_PARTIAL_PUBLICATION_OR_EXPORT',
 'versions':{'empty_request':1,'nonempty_request':2,'later_native_plan':3,'later_candidate_manifest':4,'progress_stages':10},
 'later_oracle':later,'original_acceleration':'100_FINAL_BYTE_AXIS_ACCELERATION_LIMIT_FAIL_RETAINED_EXPLICIT40_POSITIVE_Z50_UNCHANGED',
 'positive_source_sha256':'74c05263a47b85e9c87764306fee5d9c70caed4180de658393e5a034e71a6668',
 'identity_oracle_commands':17,'PID_probe_scenarios':observations,'default_candidate':provenance['default_candidate'],
 'mandatory_report':{'checks':17,'pass_run':4,'unknown_not_run':13},'guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS',
 'real_CLI_gate':cli,'isolated_bundle':isolation,'Linux':'NEW_HEAD_NOT_OBSERVED_BEFORE_PUSH','Windows':'NOT_RUN',
 'GUI':'NOT_RUN_BACKEND_CHANGE_PRIOR_GUI_HISTORICAL','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_KNOWN',
 'review':'AUTHOR_CRITICAL_REVIEW_INDEPENDENT_PENDING',
 'limits':'LOCAL_PATHS_NOT_COMPLETE_FILLED_CAP_GENERAL_SERIALIZATION_QUANTIZATION_RATES_CONTACT_HEAD_CONNECTORS_ORDER_FULL_JOB_SOFTWARE_OR_PUBLICATION_LATER_DEPARTURE_COMBINATION_UNSUPPORTED',
 'remaining':'FULL_FILLED_LAYERS_CAP_SEAMS_CONTACT_HEAD_CONNECTORS_ORDER_WHOLE_JOB_SOURCE_IMPORT_3MF_DOMAIN_GUI_HARD_RESOURCES_PROCESSTREE_ALL17_SOFTWARE_PHYSICAL_ATOMIC_PUBLICATION',
 'excluded_observations':json.loads((raw/'excluded-observations.json').read_text()),'qualified_commands':qualified,'commands':commands,
 'nonzero_commands':[c for c in commands if c['exit_code']!=0],'raw_sha256':raw_hashes,'raw_to_archive':mapping,
 'transport':'LOSSLESS_GZIP_LOG_TXT_GCODE_AND_LARGE_FILES_NO_FORCE_ADD'}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for name,digest in raw_hashes.items():
 p=root/mapping[name];h=hashlib.sha256()
 with (gzip.open(p,'rb') if p.suffix=='.gz' else p.open('rb')) as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 assert h.hexdigest()==digest,name
print(json.dumps({'status':'PASS_FROZEN_LOSSLESS_EVIDENCE','files':len(m['archive_sha256'])+1,'sources':len(files),'dependencies':len(dependencies),'binaries':len(binaries),'raw_mappings':len(mapping),'commands':len(commands),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
