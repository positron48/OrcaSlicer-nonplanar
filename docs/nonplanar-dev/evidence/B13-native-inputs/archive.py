from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();raw=root/'build/nonplanar-evidence/B13-native-inputs';dest=root/'docs/nonplanar-dev/evidence/B13-native-inputs'
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==12,len(files)
for p in files:assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==sha(root/p),p
parent=json.loads((root/'docs/nonplanar-dev/evidence/B13-build-inputs/source-manifest.json').read_text())
documents=['docs/nonplanar-dev/'+p for p in ['B13-native-inputs.md','B13-native-inputs-review.md','adr-0089-typed-native-job-inputs.md','README.md','next-tasks.md','gate-b-plan.md','status.json']]
dependencies=sorted((set(parent['source_sha256'])|set(parent['dependency_sha256'])|set(documents))-set(files))
binaries=list(parent['binary_sha256']);assert len(binaries)==4
qualified=['build2','build3-unchanged','focus1','ctest1','baseline-capture','baseline-compare','build-input-oracle','job-context-oracle','job-artifact-oracle','native-report-oracle','source-audit','package','diff-check','yaml','compiler']
qualified += [n+'-oracle' for n in ['candidate-report','candidate-report-invalid-material','candidate-report-failed-material','native-lineage-report','native-departure-report']]
qualified += [n+'-inputs-oracle' for n in ['native-lineage-report','native-departure-report']]
for name in qualified:assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
focus=E.parse(raw/'focus1.xml').getroot().find('testsuite');assert len(focus.findall('testcase'))==5 and focus.get('tests')=='226' and focus.get('failures')=='0'
ctest=E.parse(raw/'ctest1/results.xml').getroot();assert ctest.get('tests')=='458' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest1/discovery.json').read_text())['tests'])==458
for case in ctest.findall('testcase'):assert case.get('status')=='run' and all(case.find(tag) is None for tag in ('failure','error','skipped'))
comparison=json.loads((raw/'baseline-compare.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=root/'build/nonplanar-evidence/B13-native-inputs-final-off-zaa'
assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
assert sha(raw/'ctest1-jobs/compiled-build-inputs.json')==sha(root/'build/arm64/src/libslic3r/nonplanar-build-inputs/Release/inventory.json')
provenance=json.loads((raw/'provenance.json').read_text());assert provenance['exact']==50 and provenance['total']==51
assert 'Building CXX' not in (raw/'build3-unchanged.txt').read_text() and 'Linking' not in (raw/'build3-unchanged.txt').read_text()
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
m={'schema':1,'stage':'B13-native-inputs','recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT','commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B13-native-inputs.md','source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'final_build':'build2.txt.json','unchanged_build':'build3-unchanged.txt.json','focus':{'cases':5,'assertions':226,'evidence':'focus1.xml'},'ctest_final':{'tests':458,'failures':0,'skipped':0,'gate_elapsed_seconds':113.384,'evidence':'ctest1/results.xml'},'OFF_ZAA_final':comparison,'provenance':provenance,'qualified_scope':'PROTECTED_ACTUAL_TYPED_NATIVE_WORKER_INPUT_AND_DEPENDENCY_IDENTITY_ONLY','invariant':'OWNED_BODY_HEAD_SCENE_CLEARANCE_MOTION_SERIALIZER_REPLAY_AUTO_RESOURCE_ROLES_REQUIRE_COMPILED_INPUTS_REJECT_OVERRIDES_ACTUAL_STAGE_VALUES_COMPARE_NATIVE_LINEAGE_REQUIRED_ORIGINAL_ROOT_OFF_CANCEL_STALE_AND_FIXED17_BLOCK_RETAINED','compiled_inventory':provenance['compiled_inventory'],'identity_oracle_commands':11,'typed_oracle_mutation_refusals':18,'mandatory_report':{'checks':17,'pass_run':4,'unknown_not_run':13},'guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS','analytical_CLI_matrix':'NOT_RUN_THIS_CHANGE_PREVIOUS_EVIDENCE_HISTORICAL','Linux':'NEW_HEAD_NOT_OBSERVED_BEFORE_PUSH','Windows':'NOT_RUN','GUI':'NOT_RUN_FULL_PIPELINE','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_KNOWN','review':'AUTHOR_CRITICAL_REVIEW_INDEPENDENT_PENDING','remaining':'INITIAL_RESERVATION_DERIVED_ROI_PASS_HATCH_ASSEMBLY_CONTACT_SEAMS_FILL_WHOLE_ORDER_GEOMETRY_COMPLETE_SOURCE_SOFTWARE_PHYSICAL_GUI_CLI_PUBLICATION_B14_B15','historical':'First complete build retained; final incremental build includes final edited sources; unchanged build has no compile or link. No mandatory promotion, margin/contact change, golden rewrite or hash normalization.','qualified_commands':qualified,'commands':commands,'nonzero_commands':[c for c in commands if c['exit_code']!=0],'raw_sha256':raw_hashes,'raw_to_archive':mapping,'transport':'LOSSLESS_GZIP_LOG_TXT_GCODE_AND_LARGE_FILES_NO_FORCE_ADD'}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for name,digest in raw_hashes.items():
 p=root/mapping[name];h=hashlib.sha256()
 with (gzip.open(p,'rb') if p.suffix=='.gz' else p.open('rb')) as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 assert h.hexdigest()==digest,name
print(json.dumps({'status':'PASS_FROZEN_LOSSLESS_EVIDENCE','files':len(m['archive_sha256'])+1,'sources':len(files),'dependencies':len(dependencies),'binaries':len(binaries),'raw_mappings':len(mapping),'commands':len(commands),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
