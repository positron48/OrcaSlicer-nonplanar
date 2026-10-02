from pathlib import Path
import datetime, gzip, hashlib, json, platform, subprocess, xml.etree.ElementTree as E

root=Path.cwd();stage='B11-linear-candidate'
raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(), 'A new stage archive is required; do not overwrite evidence'
sha=lambda data:hashlib.sha256(data).hexdigest()
files=['src/libslic3r/Nonplanar/GCodeAdapter.cpp','src/libslic3r/Nonplanar/GCodeAdapter.hpp','src/nonplanar_verify/FullStopReplay.hpp','src/libslic3r/CMakeLists.txt','tests/nonplanar/test_serialization.cpp','tests/nonplanar/test_policy.cpp','tests/nonplanar/full_stop_oracle.hpp','docs/nonplanar-dev/adr-0069-owned-linear-candidate.md','docs/nonplanar-dev/B11-linear-candidate.md','docs/nonplanar-dev/B11-linear-candidate-review.md','docs/nonplanar-dev/B15-u1-declared-setup.md','docs/nonplanar-dev/status.json','docs/nonplanar-dev/README.md','docs/nonplanar-dev/next-tasks.md','docs/nonplanar-dev/gate-b-plan.md']
dependencies=['src/libslic3r/Nonplanar/MotionPlan.hpp','src/libslic3r/Nonplanar/MotionPlan.cpp','src/libslic3r/Nonplanar/Interval.hpp','src/libslic3r/Nonplanar/Contracts.hpp','src/libslic3r/Nonplanar/Contracts.cpp','src/libslic3r/Nonplanar/DepositionModel.cpp','src/libslic3r/Nonplanar/DepositionModel.hpp','src/libslic3r/Nonplanar/ProfileScene.cpp','src/libslic3r/Nonplanar/ProfileScene.hpp','src/libslic3r/Nonplanar/Collision.cpp','src/libslic3r/Nonplanar/Collision.hpp','src/libslic3r/Nonplanar/Canonical.hpp','src/libslic3r/Nonplanar/PlanarBody.cpp','src/libslic3r/Nonplanar/PlanarBody.hpp','src/libslic3r/Nonplanar/Policy.cpp','src/libslic3r/Nonplanar/StlImport.cpp','src/libslic3r/Nonplanar/StlImport.hpp','src/libslic3r/GCodeWriter.cpp','src/libslic3r/GCodeWriter.hpp','src/nonplanar_verify/Replay.hpp','tests/nonplanar/CMakeLists.txt','tests/fff_print/CMakeLists.txt','tests/nonplanar/material_join_oracle.hpp','scripts/nonplanar/run_logged.py','scripts/nonplanar/ctest_gate.py','scripts/nonplanar/capture_baselines.py','scripts/nonplanar/compare_baselines.py']
binaries=['build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer',
 'build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests',
 'build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests']
assert all((root/p).is_file() for p in files+dependencies+binaries)
suites={}
for path in sorted(raw.glob('*.xml')):
 s=E.parse(path).getroot().find('testsuite');assert s is not None
 suites[path.stem]={'cases':len(s.findall('testcase')),'assertions':int(s.get('tests')),
                   'failures':int(s.get('failures')),'skipped':int(s.get('skipped'))}
for name,cases,assertions in [('material1',122,48037),('body1',14,183008),('native2',1,75126),('candidate1',4,172),('serialization3',12,1524)]:
 assert suites[name]=={'cases':cases,'assertions':assertions,'failures':0,'skipped':0},name
ctest=E.parse(raw/'ctest1/results.xml').getroot()
assert ctest.get('tests')=='371' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
comparison=json.loads((raw/'baseline-compare2.txt').read_text())
assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
final_base=json.loads((root/'build/nonplanar-evidence'/f'{stage}-baselines'/'manifest.json').read_text())
assert final_base['binary_sha256']==sha((root/binaries[0]).read_bytes())
for name in ['build6','candidate1','serialization3','native2','material1','body1','ctest1','baseline-capture1','baseline-compare2',
             'package-final','source-audit-final','interval-command','diff-check-final','source-scope-final']:
 assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
line=[s for s in (raw/'interval-command.txt').read_text().splitlines() if ' -c ' in s and 'Nonplanar/GCodeAdapter.cpp' in s][-1]
assert '-fno-fast-math' in line and '-ffp-contract=off' in line and '-include' not in line
dest.mkdir(parents=True);raw_hashes={};mapping={};commands=[]
for source,prefix in [(raw,Path()),(root/'build/nonplanar-evidence'/f'{stage}-baselines',Path('baselines'))]:
 for path in sorted(source.rglob('*')):
  if not path.is_file():continue
  rel=prefix/path.relative_to(source);data=path.read_bytes();name=str(path.relative_to(root));raw_hashes[name]=sha(data)
  if path.suffix=='.json':
   record=json.loads(data)
   if isinstance(record,dict) and record.get('status')=='COMPLETED' and 'command' in record and 'exit_code' in record:
    commands.append(dict(evidence=str(rel),**record))
  if path.suffix=='.log':rel=rel.with_suffix('.txt')
  compress=path.suffix=='.gcode' or len(data)>250000
  payload=gzip.compress(data,mtime=0) if compress else data
  if compress:rel=Path(str(rel)+'.gz')
  target=dest/rel;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(payload)
  assert (gzip.decompress(payload) if compress else payload)==data;mapping[name]=str(target.relative_to(root))
(dest/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
failed=[c for c in commands if c['exit_code']!=0];assert len(failed)==5
ci=json.loads((raw/'ci-parent-runs.json').read_text())
manifest={'schema':1,'stage':stage,'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT',
 'commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B11-linear-candidate.md',
 'source_sha256':{p:sha((root/p).read_bytes()) for p in files},'dependency_sha256':{p:sha((root/p).read_bytes()) for p in dependencies},
 'binary_sha256':{p:sha((root/p).read_bytes()) for p in binaries},'platform':platform.platform(),
 'compiler':(raw/'compiler.txt').read_text().strip(),'final_build':'build6.txt.json','final_material':'material1.txt.json','final_body':'body1.txt.json','final_native':'native2.txt.json',
 'strict_interval_compile_flags':'GCodeAdapter.cpp -fno-fast-math -ffp-contract=off without PCH; MSVC registration /fp:strict',
 'software_label':'CONFIGURED_PARENT_900CC58A_NOT_CURRENT_IMPLEMENTATION_SOURCE_AND_BINARY_HASHES_BIND_ACTUAL_BUILD_FULL_B13_PROVENANCE_PENDING',
 'suites':suites,'ctest_final':{'tests':371,'failures':0,'skipped':0,'gate_elapsed_seconds':101.203},'OFF_ZAA_final':comparison,
 'runtime_contracts':{'linear_motion_plan':1,'linear_candidate':1},'persisted_contracts':'ORIGINAL_MATERIAL_SCENE_MOTION_IR_3MF_PROFILES_AND_LEDGER_HASH_FORMULAE_UNCHANGED_NO_CACHE_CHANGED_CEILINGS_CHANGE_ORDINARY_LEDGER_HASH_OLD_PROOFS_INVALID',
 'invariant':'OWNED_COMPLETE_NATIVE_FINAL_BYTES_SHA256_EXACT_POLICY_EVERY_ORIGINAL_EVENT_BYTE_RANGE_EXPLICIT_XYZ_F_E_AREA_FLOW_ONCE_PRESSURE_DWELL_AND_BARRIERS_NO_PARTIAL_PUBLICATION_NO_EXPORT_PASS',
 'model':'SYNTHETIC_KNOWN_ROUNDED_INITIAL_POSITION_MILLIMETRES_IDENTITY_TRANSFORMS_ONE_GLOBAL_ACCELERATION_BELOW_INITIAL_AND_EVERY_STEP_FULL_STOP_EACH_EVENT_NO_INSTALLED_MACHINE_QUALIFICATION',
 'bounds':'ADMITTED_NATIVE_FORMATTER_DIGITS_1_TO_9_XYZ_ERROR_HALF_QUANTUM_PLUS_CONSERVATIVE_FLOAT_ALLOWANCE_REQUIRED_ORIGINAL_CONVERSION_BUDGET_ZERO_ALLOCATION_REFUSES_RECAPTURE_EXTRA_2E_MINUS_6_UNCERTAINTY_NOT_MARGIN_REDUCTION_FINAL_GEOMETRY_AND_TIME_CERTIFICATE_PENDING',
 'work':'PLAN_POLICY_LIMITS_OWNED_BEFORE_CALLBACKS_SOURCE_CUMULATIVE_WORK_ALL_WALKS_APPENDS_HASH_PUBLICATION_BOUNDED_RECORD_BYTES_WORK_SINGLE_CALL_DEADLINE_MATERIAL_SCENE_BOTH_POLICIES_CANCEL_ROUNDING_LATE_PUBLICATION',
 'parser':'INDEPENDENT_STDLIB_ONLY_FINAL_BYTE_STATE_MACHINE_NO_PLANNER_WRITER_GCODEPROCESSOR_HEADER_XYZ_E_F_ACCEL_PRESSURE_DWELL_M400_GRAMMAR_EOF_AND_RESOURCE_GUARDS_NO_SAFETY_PASS',
 'oracles':'INDEPENDENT_113_BIT_ALL_PARSED_AXIS_CARTESIAN_COREXY_DRIVE_E_RATE_ACCEL_Q_CROSS_SECTION_PRESSURE_EVENT_RATE_AND_INDEPENDENT_NOMINAL_E_ENDPOINT_HASH_BYTE_MAP_MUTATIONS_TEST_EVIDENCE_ONLY',
 'historical_red_commands':[{k:c[k] for k in ['evidence','exit_code','command']} for c in failed],
 'historical_note':'Implementation preceded new tests. Build1 Catch decomposition and build2 mixed integer initializer compile errors. Focused serialization1 invalid new scene coverage, serialization2 incorrect assumption that unbalanced source reaches producer. Retain source admission refusal and pad only newly authored declared scene domain with original geometry/margins. Baseline comparator first receives invalid named arguments; actual positional retry passes six pairs. All five failures retained. Builds sequential; final build6 supplies tests. No production safety policy or original negative weakened.',
 'native':{'records':2218,'depositions':2092,'travel':126,'pressure_and_dwell':'SEPARATE_ANALYTICAL_FIXTURE_NOT_IN_NATIVE_SOURCE','work':37761,'final_byte_count':128890,'sha256':'d31bc1ef4ec3c4c1342d0ec2ecd89818749acf52a33ce77a1f149d32f32220a8','exact_candidate_and_sidecar':'native-final/native-full-stop.candidate.txt and native-full-stop.bytes.json','original_margin_mm':.01,'zero_conversion_source_refuses':True,'B09_exit':'UNKNOWN_ORIGINAL_MARGIN_FULL_LEDGER_NO_ROUTE_SNAPSHOT'},
 'defaults_losses_margins_timeouts_and_original_negative_tests_unchanged':True,
 'guards':'PUBLIC_GUARDED_EXPORT_BLOCK_FULL_B01_B15_AND_GATE_B_IN_PROGRESS',
 'review':'SEPARATE_AUTHOR_CRITICAL_REVIEW_INDEPENDENT_REVIEW_PENDING',
 'Linux':{'current_source':'PENDING','observed_utc':ci['observed_utc'],'runs':[{k:r[k] for k in ['id','head_sha','status','conclusion']} for r in ci['response']['workflow_runs']]},
 'Windows':'NOT_RUN','physical':'NOT_RUN_OPERATOR_PROFILE_UNCONFIRMED',
 'not_run':'OTHER_UPSTREAM_TARGETS_FULL_CAP_CONTACT_TOOL_ROI_ACCESS_ORDER_ACTUAL_TRANSFORM_FLOW_QUALIFICATION_COMPLETE_FINAL_BYTE_MATERIAL_CONTACT_SUPPORT_DOSE_AXIS_RATE_TIME_VERIFIER_FINAL_FILTERS_PROLOG_END_MOTIONS_JOB_PLATE_RESOURCE_SOFTWARE_FULL_PHYSICAL',
 'primary_dialect_sources':json.loads((raw/'dialect-primary-sources.json').read_text()),
 'commands':commands,'raw_sha256':raw_hashes,'raw_to_archive':mapping}
manifest['archive_sha256']={str(p.relative_to(root)):sha(p.read_bytes()) for p in sorted(dest.rglob('*')) if p.is_file()}
(dest/'source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
for name,h in manifest['archive_sha256'].items():assert sha((root/name).read_bytes())==h
for name,h in raw_hashes.items():
 data=(root/mapping[name]).read_bytes();data=gzip.decompress(data) if mapping[name].endswith('.gz') else data
 assert sha(data)==h
print(json.dumps({'files':len(manifest['archive_sha256'])+1,'commands':len(commands),'failed_commands':len(failed),
 'sources':len(files),'dependencies':len(dependencies),'binaries':len(binaries),'raw_byte_mappings':len(mapping),
 'archive_bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
