from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();raw=root/'build/nonplanar-evidence/B14-native-worker';dest=root/'docs/nonplanar-dev/evidence/B14-native-worker'
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==13,len(files)
for p in files:assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==sha(root/p),p
parent=json.loads((root/'docs/nonplanar-dev/evidence/B14-native-cli/source-manifest.json').read_text())
docs=['docs/nonplanar-dev/'+p for p in ['B14-native-worker.md','B14-native-worker-review.md','adr-0092-native-analysis-worker.md','README.md','next-tasks.md','gate-b-plan.md','status.json']]
dependencies=sorted((set(parent['source_sha256'])|set(parent['dependency_sha256'])|set(docs)|{'src/libslic3r/PrintObject.cpp','src/libslic3r/CustomGCode.hpp','src/libslic3r/Slicing.hpp'})-set(files))
binaries=list(parent['binary_sha256'])+['build/arm64/src/nonplanar_analysis/Release/nonplanar_analysis_worker'];assert len(binaries)==5
qualified=['build7','build8','focus3','ctest1','cli0','baseline-capture-final','baseline-compare-final','worker-focus-final-oracle','source-audit','package-final','diff-check-final','yaml-qualified','compiler','as-probe']
oracles=['build-input','job-context','job-artifact','native-report','controller','json-final','worker-final']+[n for n in ['candidate-report','candidate-report-invalid-material','candidate-report-failed-material','native-lineage-report','native-departure-report']]+['native-lineage-report-inputs','native-departure-report-inputs']
qualified += [n+'-oracle' for n in oracles];assert len(oracles)==14
for n in qualified:assert json.loads((raw/(n+'.txt.json')).read_text())['exit_code']==0,n
focus=E.parse(raw/'focus3.xml').getroot().find('testsuite');assert len(focus.findall('testcase'))==10 and focus.get('tests')=='9117' and focus.get('failures')=='0'
ctest=E.parse(raw/'ctest1/results.xml').getroot();assert ctest.get('tests')=='468' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest1/discovery.json').read_text())['tests'])==468
for c in ctest.findall('testcase'):assert c.get('status')=='run' and all(c.find(t) is None for t in ('failure','error','skipped'))
assert 'Building CXX' not in (raw/'build8.txt').read_text() and 'Linking' not in (raw/'build8.txt').read_text()
base=root/'build/nonplanar-evidence/B14-native-worker-qualified-off-zaa';old=root/'build/nonplanar-evidence/B14-native-worker-final-off-zaa'
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(r['differential']=='PASS' for r in comparison)
assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
assert sha(raw/'ctest1-jobs/compiled-build-inputs.json')==sha(root/'build/arm64/src/libslic3r/nonplanar-build-inputs/Release/inventory.json')
provenance=json.loads((raw/'provenance.json').read_text());assert provenance['exact']==50 and provenance['total']==51
cli=json.loads((raw/'cli0/manifest.json').read_text());assert len(cli['cases'])==10 and cli['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
commands=[];raw_hashes={};mapping={};dest.mkdir(parents=True)
for source,prefix in [(raw,Path()),(base,Path('baselines-final')),(old,Path('baselines0'))]:
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
m={'schema':1,'stage':'B14-native-worker','recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT','commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B14-native-worker.md','source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'final_build':'build7.txt.json','unchanged_build':'build8.txt.json','focus':{'cases':10,'assertions':9117,'evidence':'focus3.xml'},'ctest_final':{'tests':468,'failures':0,'skipped':0,'gate_elapsed_seconds':128.622,'evidence':'ctest1/results.xml'},'OFF_ZAA_final':comparison,'provenance':provenance,'compiled_inventory':provenance['compiled_inventory'],'qualified_scope':'OWNED_NATIVE_ANALYSIS_WORKER_TRANSPORT_REAL_CHILD_COMMON_BACKEND_SUPERVISION_DISPLAY_DIAGNOSTICS_ONLY','invariant':'EXACT_CANONICAL_SOURCE_MESH_MATRIX_OVERRIDE_ANNOTATION_REQUEST_HOST_TICKET_PAYLOAD_COMPILED_INVENTORY_RECAPTURE_BEFORE_PRINT_NO_PATH_REREAD_CANCEL_STALE_EXCEPTION_TIMEOUT_PROCESS_RSS_REFUSAL_NO_CANDIDATE_CREDENTIAL_FIXED17_EXPORT_BLOCK','identity_oracle_commands':14,'worker_oracle_mutation_refusals':5,'default_candidate':{'records':2098,'sha256':'c330363d909d5907a52b151a7ca499d1ce72681cc0185edd6f9c312bef12686b'},'mandatory_report':{'checks':17,'pass_run':4,'unknown_not_run':13},'guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS','real_CLI_gate':cli,'analytical_CLI_matrix':'NOT_RUN_THIS_CHANGE_HISTORICAL_EVIDENCE_RETAINED','Linux':'NEW_HEAD_NOT_OBSERVED_BEFORE_PUSH','Windows':'NOT_RUN','GUI':'NO_CALLER_RUNTIME_PACKAGING_AND_LAUNCH_POLICY_PENDING','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_KNOWN','review':'AUTHOR_CRITICAL_REVIEW_INDEPENDENT_PENDING','remaining':'MINIMAL_GUI_SERIALIZED_ADOPTION_CANCEL_REPLAY_RUNTIME_PACKAGING_ISOLATED_LAUNCH_HARD_RSS_FULL_IMPORT_3MF_FULL_CAP_LATER_PASSES_QUALIFIED_CONTACT_SEAMS_WHOLE_ORDER_GEOMETRY_SOURCE_SOFTWARE_PHYSICAL_PUBLICATION','limits':'LIVE_RSS_AND_OS_PEAK_SUPERVISED_NOT_HARD_CONTAINMENT_20MS_TASK_INFO_EXIT_TRANSITION_64KIB_REPORTING_ALLOWANCE_REFERENCED_VOLUME_MATERIAL_REFUSED','historical':'Intermediate build/assertion setup, hash encoding, referenced-material fixture, RSS exit transition and unavailable Python yaml failures retained. Final qualified source rebuilt, unchanged build has no compile/link. Original negative cases, margins, contact exceptions and goldens preserved.','qualified_commands':qualified,'commands':commands,'nonzero_commands':[c for c in commands if c['exit_code']!=0],'raw_sha256':raw_hashes,'raw_to_archive':mapping,'transport':'LOSSLESS_GZIP_LOG_TXT_GCODE_AND_LARGE_FILES_NO_FORCE_ADD'}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for name,digest in raw_hashes.items():
 p=root/mapping[name];h=hashlib.sha256()
 with (gzip.open(p,'rb') if p.suffix=='.gz' else p.open('rb')) as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 assert h.hexdigest()==digest,name
print(json.dumps({'status':'PASS_FROZEN_LOSSLESS_EVIDENCE','files':len(m['archive_sha256'])+1,'sources':len(files),'dependencies':len(dependencies),'binaries':len(binaries),'raw_mappings':len(mapping),'commands':len(commands),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
