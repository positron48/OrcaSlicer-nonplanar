from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();raw=root/'build/nonplanar-evidence/B14-native-gui';dest=root/'docs/nonplanar-dev/evidence/B14-native-gui'
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==17,len(files)
for p in files:assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==sha(root/p),p
parent=json.loads((root/'docs/nonplanar-dev/evidence/B14-native-worker/source-manifest.json').read_text())
docs=['docs/nonplanar-dev/'+p for p in ['B14-native-gui.md','B14-native-gui-review.md','adr-0093-native-analysis-view.md','README.md','next-tasks.md','gate-b-plan.md','status.json']]
dependencies=sorted((set(parent['source_sha256'])|set(parent['dependency_sha256'])|set(docs)|{'src/libslic3r/PrintObject.cpp','src/libslic3r/CustomGCode.hpp','src/libslic3r/Slicing.hpp','src/slic3r/GUI/BackgroundSlicingProcess.cpp','src/slic3r/GUI/BackgroundSlicingProcess.hpp','src/slic3r/GUI/PartPlate.cpp','src/slic3r/GUI/PartPlate.hpp','src/slic3r/GUI/Jobs/Worker.hpp','src/slic3r/GUI/Jobs/Job.hpp','src/slic3r/GUI/Jobs/BoostThreadWorker.cpp','src/slic3r/GUI/Jobs/BoostThreadWorker.hpp','src/libslic3r/PresetBundle.cpp','src/libslic3r/PresetBundle.hpp','src/slic3r/GUI/GUI_App.hpp','src/slic3r/GUI/DeviceManager.cpp','src/slic3r/GUI/DeviceManager.hpp','src/slic3r/GUI/UserManager.cpp','src/slic3r/GUI/UserManager.hpp'})-set(files))
binaries=list(parent['binary_sha256']);assert len(binaries)==5
qualified=['build12','build13','focus3','ctest3','cli1','baseline-capture-final','baseline-compare-final','source-audit-final','package-final','diff-check-final','yaml-final','compiler','python-final','bash-final','bundle-delivery-prepare','bundle-delivery-isolation','gui-observation-gate1','view-delivery-oracle']
oracles=['build-input','job-context','job-artifact','native-report','controller','json-final','worker-final','candidate-report','candidate-report-invalid-material','candidate-report-failed-material','native-lineage-report','native-departure-report','native-lineage-report-inputs','native-departure-report-inputs']
qualified += [n+'-final-oracle' for n in oracles];assert len(oracles)==14
for n in qualified:assert json.loads((raw/(n+'.txt.json')).read_text())['exit_code']==0,n
focus=E.parse(raw/'focus3.xml').getroot().find('testsuite');assert len(focus.findall('testcase'))==12 and focus.get('tests')=='9270' and focus.get('failures')=='0'
ctest=E.parse(raw/'ctest3/results.xml').getroot();assert ctest.get('tests')=='470' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest3/discovery.json').read_text())['tests'])==470
for c in ctest.findall('testcase'):assert c.get('status')=='run' and all(c.find(t) is None for t in ('failure','error','skipped'))
assert 'Building CXX' not in (raw/'build13.txt').read_text() and 'Linking' not in (raw/'build13.txt').read_text()
base=root/'build/nonplanar-evidence/B14-native-gui-qualified-off-zaa'
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(r['differential']=='PASS' for r in comparison)
assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
assert sha(raw/'ctest3-jobs/compiled-build-inputs.json')==sha(root/'build/arm64/src/libslic3r/nonplanar-build-inputs/Release/inventory.json')
provenance=json.loads((raw/'provenance.json').read_text());assert provenance['exact']==50 and provenance['total']==51
cli=json.loads((raw/'cli1/manifest.json').read_text());assert len(cli['cases'])==10 and cli['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
gui=json.loads((raw/'gui-observation.json').read_text());assert gui['status']=='PASS_OBSERVED_NATIVE_GUI_WITH_FINAL_BINARY_WORKER_BINDING'
assert gui['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
assert gui['worker_sha256']==sha(root/'build/arm64/src/nonplanar_analysis/Release/nonplanar_analysis_worker')
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
m={'schema':1,'stage':'B14-native-gui','recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT','commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B14-native-gui.md','source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'final_build':'build12.txt.json','unchanged_build':'build13.txt.json','focus':{'cases':12,'assertions':9270,'evidence':'focus3.xml'},'ctest_final':{'tests':470,'failures':0,'skipped':0,'gate_elapsed_seconds':138.863,'evidence':'ctest3/results.xml'},'OFF_ZAA_final':comparison,'provenance':provenance,'compiled_inventory':provenance['compiled_inventory'],'qualified_scope':'NATIVE_GUI_EXACT_OWNED_INPUTS_SERIALIZED_DISPLAY_ADOPTION_ACTUAL_CHILD_REPORT_MOVEMENT_REPLAY_EDIT_CANCEL_CLOSE_TRUSTED_BUNDLE_ISOLATED_LAUNCH_ONLY','invariant':'EXACT_CANONICAL_SOURCE_MESH_MATRIX_OVERRIDE_ANNOTATION_REQUEST_HOST_TICKET_PAYLOAD_COMPILED_INVENTORY_RECAPTURE_BEFORE_PRINT_NO_PATH_REREAD_CANCEL_STALE_EXCEPTION_TIMEOUT_PROCESS_RSS_REFUSAL_NO_CANDIDATE_CREDENTIAL_FIXED17_EXPORT_BLOCK','identity_oracle_commands':15,'worker_oracle_mutation_refusals':5,'default_candidate':{'records':2098,'sha256':'c330363d909d5907a52b151a7ca499d1ce72681cc0185edd6f9c312bef12686b'},'mandatory_report':{'checks':17,'pass_run':4,'unknown_not_run':13},'guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS','real_CLI_gate':cli,'analytical_CLI_matrix':'NOT_RUN_THIS_CHANGE_HISTORICAL_EVIDENCE_RETAINED','Linux':'NEW_HEAD_NOT_OBSERVED_BEFORE_PUSH','Windows':'NOT_RUN','GUI':'ACTUAL_FINAL_MACOS_SYNTHETIC_MINIMAL_VIEW_PASS_FULL_P2_PENDING','GUI_observation':gui,'physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_KNOWN','review':'AUTHOR_CRITICAL_REVIEW_INDEPENDENT_PENDING','remaining':'HARD_PARENT_CHILD_CONTAINMENT_FULL_IMPORT_SOURCE_3MF_CAP_HEAD_MATERIAL_WITNESS_ROLES_TIME_SEEKING_COMPLETE_CAP_LATER_PASSES_QUALIFIED_CONTACT_SEAMS_WHOLE_JOB_SOURCE_SOFTWARE_PHYSICAL_PUBLICATION','limits':'PARENT_SOURCE_CAPTURE_PRIVATE_PRINT_APPLY_COOPERATIVE_CHILD_RSS_SUPERVISED_NOT_HARD_CONTAINMENT_REFERENCED_VOLUME_MATERIAL_IDS_REFUSED','historical':'Intermediate build dependency/header/const failures, actual plate/config/source-transform refusals, Keychain and idle-event-loop GUI samples and SHA-encoding checker refusal retained. Final source rebuilt; unchanged build has no compile/link. Final GUI synthetic fixture independently checks actual source/placement and final-byte readout. Original negatives, margins, contact exceptions and goldens preserved.','qualified_commands':qualified,'commands':commands,'nonzero_commands':[c for c in commands if c['exit_code']!=0],'raw_sha256':raw_hashes,'raw_to_archive':mapping,'transport':'LOSSLESS_GZIP_LOG_TXT_GCODE_AND_LARGE_FILES_NO_FORCE_ADD'}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for name,digest in raw_hashes.items():
 p=root/mapping[name];h=hashlib.sha256()
 with (gzip.open(p,'rb') if p.suffix=='.gz' else p.open('rb')) as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 assert h.hexdigest()==digest,name
print(json.dumps({'status':'PASS_FROZEN_LOSSLESS_EVIDENCE','files':len(m['archive_sha256'])+1,'sources':len(files),'dependencies':len(dependencies),'binaries':len(binaries),'raw_mappings':len(mapping),'commands':len(commands),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
