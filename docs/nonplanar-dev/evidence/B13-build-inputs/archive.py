from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();raw=root/'build/nonplanar-evidence/B13-build-inputs';dest=root/'docs/nonplanar-dev/evidence/B13-build-inputs'
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==13
for p in files:assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==sha(root/p),p
parent=json.loads((root/'docs/nonplanar-dev/evidence/B04-partition-order/source-manifest.json').read_text())
dependencies=sorted((set(parent['source_sha256'])|set(parent['dependency_sha256']))-set(files))
binaries=list(parent['binary_sha256']);assert len(binaries)==4
qualified=['build7','build8-unchanged','generator-final3','focus2','ctest2','baseline-capture2','baseline-compare2','build-input-oracle-final','job-context-oracle-final','job-artifact-oracle-final','native-report-oracle-final','source-audit-final','package-final','diff-check','yaml','compiler']
qualified+= [n+'-oracle-final' for n in ['candidate-report','candidate-report-invalid-material','candidate-report-failed-material','native-lineage-report','native-departure-report']]
for name in qualified:assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
focus=E.parse(raw/'focus2.xml').getroot().find('testsuite');assert len(focus.findall('testcase'))==10 and focus.get('tests')=='230' and focus.get('failures')=='0'
ctest=E.parse(raw/'ctest2/results.xml').getroot();assert ctest.get('tests')=='453' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest2/discovery.json').read_text())['tests'])==453
for case in ctest.findall('testcase'):assert case.get('status')=='run' and all(case.find(tag) is None for tag in ('failure','error','skipped'))
comparison=json.loads((raw/'baseline-compare2.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=root/'build/nonplanar-evidence/B13-build-inputs-final-off-zaa'
assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
assert sha(raw/'focus2-jobs/compiled-build-inputs.json')==sha(raw/'ctest2-jobs/compiled-build-inputs.json')
assert sha(raw/'ctest2-jobs/compiled-build-inputs.json')==sha(root/'build/arm64/src/libslic3r/nonplanar-build-inputs/Release/inventory.json')
provenance=json.loads((raw/'provenance.json').read_text());assert provenance['exact']==50 and provenance['total']==51
assert 'Building CXX' not in (raw/'build8-unchanged.txt').read_text() and 'Linking' not in (raw/'build8-unchanged.txt').read_text()
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
m={'schema':1,'stage':'B13-build-inputs','recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT','commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B13-build-inputs.md','source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'final_build':'build7.txt.json','unchanged_build':'build8-unchanged.txt.json','focus':{'cases':10,'assertions':230,'evidence':'focus2.xml'},'ctest_final':{'tests':453,'failures':0,'skipped':0,'elapsed_seconds':110.587,'evidence':'ctest2/results.xml'},'OFF_ZAA_final':comparison,'provenance':provenance,'qualified_scope':'COMPILED_BUILD_INPUT_INVENTORY_AND_PROTECTED_NATIVE_JOB_RESOURCE_IDENTITY_ONLY','invariant':'NATIVE_COMPILED_MODE_FACTORY_OWNS_EXACT_SOFTWARE_RESOURCE_DUPLICATE_OVERRIDE_REJECTED_ORIGINAL_V1_DECLARED_OPAQUE_RESOURCE_BOUNDS_CANCEL_STALE_OFF_AND_FIXED17_BLOCK_RETAINED','compiled_inventory':provenance['compiled_inventory'],'generator_tests':8,'mandatory_report':{'checks':17,'pass_run':4,'unknown_not_run':13},'guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS','software_identity':'UNKNOWN_NOT_RUN_OBJECT_SOURCE_LINKAGE_INSTALLED_DEPENDENCY_RUNTIME_RESOURCE_AND_COMPLETE_FLAG_PROVENANCE_UNQUALIFIED','analytical_CLI_matrix':'NOT_RUN_THIS_CHANGE_PREVIOUS_EVIDENCE_HISTORICAL','Linux':'NEW_HEAD_NOT_OBSERVED_BEFORE_PUSH','Windows':'NOT_RUN','GUI':'NOT_RUN_FULL_PIPELINE','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_KNOWN_NO_PRINTER_PRESET_FIRMWARE_CLOUD_EDITS','review':'AUTHOR_AUDIT_INDEPENDENT_REVIEW_PENDING','historical':'Controlled first build interruption exit130 to raise parallelism; Make coarse timestamp object and link regressions retained; existing static-cycle object-library generation failure fixed; external-output guard missing-leaf symlink fixture failures corrected. No mandatory check promoted, no hash normalization or proof reuse.','qualified_commands':qualified,'commands':commands,'nonzero_commands':[c for c in commands if c['exit_code']!=0],'raw_sha256':raw_hashes,'raw_to_archive':mapping,'transport':'LOSSLESS_GZIP_LOG_TXT_GCODE_AND_LARGE_FILES_NO_FORCE_ADD'}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for name,digest in raw_hashes.items():
 p=root/mapping[name];h=hashlib.sha256()
 with (gzip.open(p,'rb') if p.suffix=='.gz' else p.open('rb')) as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 assert h.hexdigest()==digest,name
print(json.dumps({'status':'PASS_FROZEN_LOSSLESS_EVIDENCE','files':len(m['archive_sha256'])+1,'sources':len(files),'dependencies':len(dependencies),'binaries':len(binaries),'raw_mappings':len(mapping),'commands':len(commands),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
