from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();stage='B12-final-rates';raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(),'Never overwrite an existing stage archive'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
files=['.github/workflows/nonplanar-native.yml','src/CMakeLists.txt','src/libslic3r/Nonplanar/GCodeAdapter.cpp','src/libslic3r/Nonplanar/GCodeAdapter.hpp','src/nonplanar_verify/CMakeLists.txt','src/nonplanar_verify/LinearRates.cpp','src/nonplanar_verify/LinearRates.hpp','src/nonplanar_verify/audit.cpp','scripts/nonplanar/validate_rate_audit_cli.py','tests/nonplanar/CMakeLists.txt','tests/fff_print/CMakeLists.txt','tests/nonplanar/test_serialization.cpp','tests/nonplanar/test_policy.cpp','docs/nonplanar-dev/adr-0070-independent-final-rates.md','docs/nonplanar-dev/B12-final-rates.md','docs/nonplanar-dev/B12-final-rates-review.md','docs/nonplanar-dev/README.md','docs/nonplanar-dev/status.json','docs/nonplanar-dev/gate-b-plan.md','docs/nonplanar-dev/next-tasks.md']
dependencies=['src/nonplanar_verify/FullStopReplay.hpp','src/nonplanar_verify/Replay.hpp','tests/nonplanar/full_stop_oracle.hpp','tests/nonplanar/material_join_oracle.hpp','src/libslic3r/Nonplanar/MotionPlan.hpp','src/libslic3r/Nonplanar/MotionPlan.cpp','src/libslic3r/Nonplanar/Interval.hpp','src/libslic3r/Nonplanar/Contracts.hpp','src/libslic3r/Nonplanar/Contracts.cpp','src/libslic3r/Nonplanar/DepositionModel.cpp','src/libslic3r/Nonplanar/DepositionModel.hpp','src/libslic3r/Nonplanar/ProfileScene.cpp','src/libslic3r/Nonplanar/ProfileScene.hpp','src/libslic3r/Nonplanar/Collision.cpp','src/libslic3r/Nonplanar/Collision.hpp','src/libslic3r/Nonplanar/Canonical.hpp','src/libslic3r/Nonplanar/PlanarBody.cpp','src/libslic3r/Nonplanar/PlanarBody.hpp','src/libslic3r/Nonplanar/Policy.cpp','src/libslic3r/Nonplanar/StlImport.cpp','src/libslic3r/Nonplanar/StlImport.hpp','src/libslic3r/GCodeWriter.cpp','src/libslic3r/GCodeWriter.hpp','src/libslic3r/CMakeLists.txt','CMakeLists.txt','deps/Boost/Boost.cmake','deps_src/nlohmann/json.hpp','scripts/nonplanar/run_logged.py','scripts/nonplanar/ctest_gate.py','scripts/nonplanar/capture_baselines.py','scripts/nonplanar/compare_baselines.py']
binaries=['build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer','build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests','build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests','build/arm64/src/nonplanar_verify/Release/nonplanar_rate_audit']
assert all((root/p).is_file() for p in files+dependencies+binaries)
suites={}
for p in sorted(raw.glob('*.xml')):
 s=E.parse(p).getroot().find('testsuite')
 suites[p.stem]={'cases':len(s.findall('testcase')) if s is not None else 0,'assertions':int(s.get('tests')) if s is not None else 0,'failures':int(s.get('failures')) if s is not None else 0,'skipped':int(s.get('skipped')) if s is not None else 0}
for name,cases,assertions in [('rates5',7,137),('material3',122,48037),('body3',14,183013),('native3',1,75125),('native4',1,75131),('serialization3',12,1524)]:
 assert suites[name]=={'cases':cases,'assertions':assertions,'failures':0,'skipped':0},name
ctest=E.parse(raw/'ctest1/results.xml').getroot();assert ctest.get('tests')=='378' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
comparison=json.loads((raw/'baseline-compare1.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=json.loads((root/'build/nonplanar-evidence'/f'{stage}-baselines'/'manifest.json').read_text());assert base['binary_sha256']==sha(root/binaries[0])
for name in ['build6','rates5','cli3','native3','native4','native-cli','serialization3','material3','body3','ctest1','baseline-capture1','baseline-compare1','package-final','source-audit-final','strict-command','headless-link-command','diff-check-final','source-scope-final','compiler']:
 assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
strict=[s for s in (raw/'strict-command.txt').read_text().splitlines() if ' -c ' in s and 'LinearRates.cpp' in s][-1]
assert '-fno-fast-math' in strict and '-ffp-contract=off' in strict and '-include' not in strict
link=(raw/'headless-link-command.txt').read_text().splitlines()[-1];assert 'nonplanar_rate_audit' in link and 'libslic3r' not in link and 'libGUI' not in link
cli=json.loads((raw/'cli3/manifest.json').read_text());assert cli['status']=='PASS' and len(cli['cases'])==10
native=json.loads((raw/'native-rate-input-binding.json').read_text());assert native['cumulative_work']==66507 and native['records']==2218
for p,h in native['sha256'].items():assert sha(raw/p)==h
sidecar=json.loads((raw/'native-final/native-full-stop.bytes.json').read_text());assert sidecar['sha256']==sha(raw/'native-final/native-full-stop.candidate.txt')
assert sidecar['bytes']==128890 and sidecar['records']==2218 and sidecar['native_B09_exit']=='UNKNOWN' and sidecar['export']=='BLOCK'
dest.mkdir(parents=True);raw_hashes={};mapping={};commands=[]
for source,prefix in [(raw,Path()),(root/'build/nonplanar-evidence'/f'{stage}-baselines',Path('baselines'))]:
 for path in sorted(source.rglob('*')):
  if not path.is_file():continue
  rel=prefix/path.relative_to(source);name=str(path.relative_to(root));raw_hashes[name]=sha(path)
  if path.suffix=='.json':
   record=json.loads(path.read_text())
   if isinstance(record,dict) and record.get('status')=='COMPLETED' and 'command' in record and 'exit_code' in record:commands.append(dict(evidence=str(rel),**record))
  if path.suffix=='.log':rel=rel.with_suffix('.txt')
  compressed=path.suffix=='.gcode' or path.stat().st_size>250000
  if compressed:rel=Path(str(rel)+'.gz')
  target=dest/rel;target.parent.mkdir(parents=True,exist_ok=True)
  if compressed:
   with path.open('rb') as src,target.open('wb') as dst,gzip.GzipFile(filename='',mode='wb',fileobj=dst,mtime=0) as gz:shutil.copyfileobj(src,gz,1024*1024)
  else:shutil.copyfile(path,target)
  mapping[name]=str(target.relative_to(root))
(dest/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
failed=[c for c in commands if c['exit_code']!=0];assert len(failed)==7,len(failed)
ci=json.loads((raw/'ci-parent-runs.json').read_text())
m={'schema':1,'stage':stage,'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT','commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B12-final-rates.md',
'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'final_build':'build6.txt.json','suites':suites,'ctest_final':{'tests':378,'failures':0,'skipped':0,'gate_elapsed_seconds':93.115},'OFF_ZAA_final':comparison,'headless_final':'cli3/manifest.json and native-cli.txt; component-only exits, every report job UNKNOWN/export false',
'runtime_contracts':{'linear_motion_plan':1,'linear_candidate':1,'linear_rates':1},'persisted_contracts':'ORIGINAL_IR_MATERIAL_SCENE_PROFILE_3MF_LEDGER_FINGERPRINTS_UNCHANGED_NO_CACHE_NEW_DIAGNOSTIC_ONLY',
'invariant':'OWNED_EXACT_FINAL_DECIMAL_BYTES_EXACT_BINARY64_POLICY_RATIONAL_AXIS_CARTESIAN_COREXY_DRIVE_E_Q_CROSS_SECTION_ACCELERATION_RETRACTION_EVENT_RATE_DECISIONS_ENCLOSED_PI_SQRT_DOSE_IDEAL_TIME_AND_FINAL_PRESSURE_DEBT_NO_PLANNER_WRITER_GCODEPROCESSOR_DECISION',
'model':'SYNTHETIC_UNCONFIRMED_KNOWN_INITIAL_STATE_IDENTITY_TRANSFORMS_MILLIMETRES_DECLARED_IDEAL_FULL_STOPS_ONLY_NOT_ACTUAL_WALL_TIME_OR_DELIVERED_VOLUME_CALIBRATION',
'work':'ALL_INPUTS_OWNED_BEFORE_CALLBACKS_CANDIDATE_CUMULATIVE_WORK_BOUNDED_BYTES_RECORDS_OPERATIONS_SINGLE_ABSOLUTE_CALL_DEADLINE_CANCEL_SOURCE_SCENE_BOTH_POLICIES_ROUNDING_GRADUAL_UNDERFLOW_FINAL_PUBLICATION_NO_PARTIAL_SNAPSHOT_COOPERATIVE_NOT_HARD_PROCESS_SANDBOX',
'strict_flags':'LinearRates.cpp -fno-fast-math -ffp-contract=off no PCH; MSVC registered /fp:strict','standalone_link':'independent static library only, Boost and JSON headers, no slicer/GUI libraries',
'software_label':'CONFIGURED_PARENT_69C4B339_NOT_CURRENT_IMPLEMENTATION_SOURCE_AND_BINARY_HASHES_BIND_ACTUAL_TESTED_BUILD_FULL_B13_PROVENANCE_PENDING',
'native':{'records':2218,'depositions':2092,'travel':126,'bytes':128890,'sha256':sidecar['sha256'],'candidate_work':37761,'cumulative_work':66507,'standalone_work':28746,'ideal_time_seconds':native['ideal_mechanical_duration_s'],'pressure_dwell':'SEPARATE_ANALYTICAL_AND_CLI_FIXTURES_NOT_NATIVE_SOURCE','exact_candidate':'native-final/native-full-stop.candidate.txt','policy':'native-final/native-rate-policy.json; explicit field transcription from actual native fixture and rounded initial sidecar','original_margin_mm':.01,'all_head_parts':7,'tip_z_mm':0,'B09_exit':'UNKNOWN_NO_ROUTE_SNAPSHOT'},
'historical_red_commands':[{k:c[k] for k in ['evidence','exit_code','command']} for c in failed],
'historical_note':'Initial missing header build and missing CLI executable are scaffold failures, not behavioral red/green. Initial four tests follow implementation. CLI excess axis array and event cadence tests precede fixes and reproduce real new defects. New pressure-red incorrectly assumes final restore required; it passes after accidental stricter code, then final-state-red proves the regression against original ADR0069. Final code reports exact allowed final debt and retains every original negative/state admission/margin. Native2 wrong executable runs zero cases exit2; final native3/native4 are actual successful cases. Historical XML/CLI partial evidence is retained separately, never counted as final verification.',
'defaults_losses_margins_timeouts_original_negative_tests_unchanged':True,'guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS','review':'SEPARATE_AUTHOR_CRITICAL_REVIEW_INDEPENDENT_PENDING','Linux':{'current_source':'PENDING_NEW_RUN_AFTER_PUSH','observed_utc':ci['observed_utc'],'runs':[{k:r[k] for k in ['id','head_sha','status','conclusion']} for r in ci['response']['workflow_runs']]},'Windows':'NOT_RUN','physical':'NOT_RUN_OPERATOR_PROFILE_UNCONFIRMED',
'not_implemented_or_not_run':'COMPLETE_FINAL_BYTE_MATERIAL_CONTACT_SUPPORT_DELIVERED_VOLUME_UNCERTAINTY_GEOMETRY_ROUNDING_SCENE_AND_ROUTE_TRANSFORMS_CALIBRATED_FLOW_PRESSURE_ADVANCE_FINAL_FILTERS_PROLOG_END_MOTIONS_COMPLETE_JOB_PLATE_RESOURCE_SOFTWARE_AND_REPORT_HASH_BINDING_FULL_CAP_CONTACT_TOOL_ACCESS_ORDER_ACTUAL_PRINTER_TIME_INDEPENDENT_REVIEW_PLATFORMS_PHYSICAL',
'primary_arithmetic_source':json.loads((raw/'arithmetic-primary-source.json').read_text()),'commands':commands,'raw_sha256':raw_hashes,'raw_to_archive':mapping}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()}
(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for name,h in m['archive_sha256'].items():assert sha(root/name)==h
for name,h in raw_hashes.items():
 path=root/mapping[name];check=hashlib.sha256()
 with (gzip.open(path,'rb') if path.suffix=='.gz' else path.open('rb')) as data:
  for block in iter(lambda:data.read(1024*1024),b''):check.update(block)
 assert check.hexdigest()==h,name
print(json.dumps({'files':len(m['archive_sha256'])+1,'commands':len(commands),'failed_commands':len(failed),'sources':len(files),'dependencies':len(dependencies),'binaries':len(binaries),'raw_byte_mappings':len(mapping),'archive_bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
