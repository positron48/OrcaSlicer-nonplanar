from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();raw=root/'build/nonplanar-evidence/B14-native-cli';dest=root/'docs/nonplanar-dev/evidence/B14-native-cli'
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==9,len(files)
for p in files:assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==sha(root/p),p
parent=json.loads((root/'docs/nonplanar-dev/evidence/B14-native-controller/source-manifest.json').read_text())
documents=['docs/nonplanar-dev/'+p for p in ['B14-native-cli.md','B14-native-cli-review.md','adr-0091-native-analysis-cli-transport.md','native-analysis-transport.md','README.md','next-tasks.md','gate-b-plan.md','status.json']]
dependencies=sorted((set(parent['source_sha256'])|set(parent['dependency_sha256'])|set(documents))-set(files))
binaries=list(parent['binary_sha256']);assert len(binaries)==4
qualified=['build7','build8','focus4','ctest0','cli11','json-focus-final-oracle','baseline-capture','baseline-compare','build-input-oracle','job-context-oracle','job-artifact-oracle','native-report-oracle','source-audit','package','diff-check','yaml-qualified','compiler']
qualified += [n+'-oracle' for n in ['candidate-report','candidate-report-invalid-material','candidate-report-failed-material','native-lineage-report','native-departure-report']]
qualified += [n+'-inputs-oracle' for n in ['native-lineage-report','native-departure-report']]
qualified += ['controller-oracle','json-final-oracle']
for name in qualified:assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
focus=E.parse(raw/'focus4.xml').getroot().find('testsuite');assert len(focus.findall('testcase'))==5 and focus.get('tests')=='8972' and focus.get('failures')=='0'
ctest=E.parse(raw/'ctest0/results.xml').getroot();assert ctest.get('tests')=='463' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest0/discovery.json').read_text())['tests'])==463
for case in ctest.findall('testcase'):assert case.get('status')=='run' and all(case.find(tag) is None for tag in ('failure','error','skipped'))
comparison=json.loads((raw/'baseline-compare.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=root/'build/nonplanar-evidence/B14-native-cli-final-off-zaa'
assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
assert sha(raw/'ctest0-jobs/compiled-build-inputs.json')==sha(root/'build/arm64/src/libslic3r/nonplanar-build-inputs/Release/inventory.json')
provenance=json.loads((raw/'provenance.json').read_text());assert provenance['exact']==50 and provenance['total']==51
assert 'Building CXX' not in (raw/'build8.txt').read_text() and 'Linking' not in (raw/'build8.txt').read_text()
cli=json.loads((raw/'cli11/manifest.json').read_text());assert len(cli['cases'])==10 and cli['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
commands=[];raw_hashes={};mapping={};dest.mkdir(parents=True)
for source,prefix in [(raw,Path()),(base,Path('baselines-final'))]:
 for path in sorted(source.rglob('*')):
  if not path.is_file() or path.name.startswith('archive-execution.'):continue
  rel=prefix/path.relative_to(source);name=str(path.relative_to(root));raw_hashes[name]=sha(path)
  if path.suffix=='.json':
   try:entry=json.loads(path.read_text())
   except json.JSONDecodeError:entry=None
   if isinstance(entry,dict) and entry.get('status')=='COMPLETED' and 'command' in entry:commands.append(dict(evidence=str(rel),**entry))
  compressed=path.suffix in ('.log','.txt','.gcode') or path.stat().st_size>250000
  if compressed:rel=Path(str(rel)+'.gz')
  target=dest/rel;assert not target.exists();target.parent.mkdir(parents=True,exist_ok=True)
  if compressed:
   with path.open('rb') as src,target.open('wb') as dst,gzip.GzipFile(filename='',mode='wb',fileobj=dst,mtime=0) as gz:shutil.copyfileobj(src,gz,1048576)
  else:shutil.copyfile(path,target)
  mapping[name]=str(target.relative_to(root))
(dest/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
m={'schema':1,'stage':'B14-native-cli','recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT','commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B14-native-cli.md','source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'final_build':'build7.txt.json','unchanged_build':'build8.txt.json','focus':{'cases':5,'assertions':8972,'evidence':'focus4.xml'},'ctest_final':{'tests':463,'failures':0,'skipped':0,'gate_elapsed_seconds':118.190,'evidence':'ctest0/results.xml'},'OFF_ZAA_final':comparison,'provenance':provenance,'qualified_scope':'REAL_MAIN_ORCA_NATIVE_CLI_EDITING_TRANSPORT_OWNED_REQUEST_FINAL_BYTE_MOVEMENT_DIAGNOSTICS_ONLY','invariant':'EXACT_EDITING_TRANSPORT_PRIVATE_REQUEST_FACTORY_REAL_MAIN_CLI_PRODUCTION_DEFAULTS_ACTUAL_JOB_REPORT_MOVEMENTS_NO_MIXED_ACTION_PROGRESS_SIGINT_NO_CANDIDATE_EXPORT_FIXED17_ORIGINAL_LIMITS_OFF_ZAA_RETAINED','compiled_inventory':provenance['compiled_inventory'],'identity_oracle_commands':13,'request_oracle_mutation_refusals':7,'focus_request_oracle':True,'controller_candidate':{'records':2197,'bytes':127589,'sha256':'e338ae9a6da37420573361725b6ce3b27691edce4e78de1c37d86e11a14a3739','replay_evaluations':144333},'typed_oracle_mutation_refusals':18,'mandatory_report':{'checks':17,'pass_run':4,'unknown_not_run':13},'guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS','analytical_CLI_matrix':'NOT_RUN_THIS_CHANGE_PREVIOUS_EVIDENCE_HISTORICAL','Linux':'NEW_HEAD_NOT_OBSERVED_BEFORE_PUSH','Windows':'NOT_RUN','GUI':'NOT_RUN_FULL_PIPELINE','real_CLI_gate':json.loads((raw/'cli11/manifest.json').read_text()),'transport_oracle_mutation_refusals':10,'default_cli_reference':{'records':2098,'sha256':'c330363d909d5907a52b151a7ca499d1ce72681cc0185edd6f9c312bef12686b','replay_evaluations':137782},'physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_KNOWN','review':'AUTHOR_CRITICAL_REVIEW_INDEPENDENT_PENDING','remaining':'GUI_WORKER_PROCESS_RSS_ISOLATION_COMMON_WORK_FULL_IMPORT_DIAGNOSTICS_SOURCE_CAPTURE_FULL_CAP_LATER_PASSES_CONTACT_SEAMS_WHOLE_JOB_ORDER_GEOMETRY_SOURCE_SOFTWARE_PHYSICAL_3MF_PUBLICATION_B14_B15','historical':'Intermediate build/test/preset preparation failures retained. Final incremental build includes final source; unchanged build has no compile or link. Actual main CLI uses production defaults, separate from original finer fixture. Linux CI gates configured, execution pending. No mandatory promotion, margin/contact change, golden rewrite or hash normalization.','qualified_commands':qualified,'commands':commands,'nonzero_commands':[c for c in commands if c['exit_code']!=0],'raw_sha256':raw_hashes,'raw_to_archive':mapping,'transport':'LOSSLESS_GZIP_LOG_TXT_GCODE_AND_LARGE_FILES_NO_FORCE_ADD'}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for name,digest in raw_hashes.items():
 p=root/mapping[name];h=hashlib.sha256()
 with (gzip.open(p,'rb') if p.suffix=='.gz' else p.open('rb')) as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 assert h.hexdigest()==digest,name
print(json.dumps({'status':'PASS_FROZEN_LOSSLESS_EVIDENCE','files':len(m['archive_sha256'])+1,'sources':len(files),'dependencies':len(dependencies),'binaries':len(binaries),'raw_mappings':len(mapping),'commands':len(commands),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
