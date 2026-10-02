from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();stage='B12-final-material';raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(),'Never overwrite an existing stage archive'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
files=['.github/workflows/nonplanar-native.yml','src/libslic3r/Nonplanar/GCodeAdapter.cpp','src/libslic3r/Nonplanar/GCodeAdapter.hpp','src/nonplanar_verify/CMakeLists.txt','src/nonplanar_verify/LinearRates.cpp','src/nonplanar_verify/audit.cpp','src/nonplanar_verify/Exact.hpp','src/nonplanar_verify/LinearMaterial.cpp','src/nonplanar_verify/LinearMaterial.hpp','src/nonplanar_verify/MaterialJson.hpp','tests/fff_print/CMakeLists.txt','tests/nonplanar/test_policy.cpp','tests/nonplanar/test_serialization.cpp','tests/nonplanar/test_final_material.cpp','scripts/nonplanar/validate_material_audit_cli.py','docs/nonplanar-dev/adr-0071-final-material-replay.md','docs/nonplanar-dev/B12-final-material.md','docs/nonplanar-dev/B12-final-material-review.md','docs/nonplanar-dev/README.md','docs/nonplanar-dev/status.json','docs/nonplanar-dev/gate-b-plan.md','docs/nonplanar-dev/next-tasks.md']
parent=json.loads((root/'docs/nonplanar-dev/evidence/B12-final-rates/source-manifest.json').read_text())
dependencies=list(parent['dependency_sha256'])+['src/nonplanar_verify/LinearRates.hpp','src/CMakeLists.txt','tests/nonplanar/CMakeLists.txt','scripts/nonplanar/validate_rate_audit_cli.py']
binaries=list(parent['binary_sha256'])
assert len(files)==22 and len(set(files+dependencies))==len(files+dependencies)
assert all((root/p).is_file() for p in files+dependencies+binaries)
suites={}
for p in sorted(raw.glob('*.xml')):
 s=E.parse(p).getroot().find('testsuite')
 suites[p.stem]={'cases':len(s.findall('testcase')) if s is not None else 0,'assertions':int(s.get('tests')) if s is not None else 0,'failures':int(s.get('failures')) if s is not None else 0,'skipped':int(s.get('skipped')) if s is not None else 0}
for name,cases,assertions in [('material-focus-final',4,106),('rate-focus-final',8,144),('serialization-final',24,1774),('material-final',122,48037),('body-final',14,183026),('native3',1,75146)]:
 assert suites[name]=={'cases':cases,'assertions':assertions,'failures':0,'skipped':0},name
ctest=E.parse(raw/'ctest-final/results.xml').getroot();assert ctest.get('tests')=='383' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=json.loads((root/'build/nonplanar-evidence'/f'{stage}-baselines'/'manifest.json').read_text());assert base['binary_sha256']==sha(root/binaries[0])
for p,h in json.loads((raw/'preformat-binary-sha256.json').read_text()).items():assert sha(root/p)==h
format_audit=json.loads((raw/'format-binary-audit.json').read_text());assert all(format_audit['binary_identical'].values())
final_commands=['build6','material-focus-final','rate-focus-final','serialization-final','material-final','body-final','native3','ctest-final','material-cli1','rate-cli1','native-cli2','baseline-capture-final','baseline-compare-final','package-final','source-audit-final','strict-command','strict-rate-command','headless-link-command-release','diff-check-final','compiler']
for name in final_commands:assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
for log,source in [('strict-command.txt','LinearMaterial.cpp'),('strict-rate-command.txt','LinearRates.cpp')]:
 strict=[s for s in (raw/log).read_text().splitlines() if ' -c ' in s and source in s][-1]
 assert '-fno-fast-math' in strict and '-ffp-contract=off' in strict and '-include' not in strict
link=(raw/'headless-link-command-release.txt').read_text().splitlines()[-1];assert 'nonplanar_rate_audit' in link and 'libslic3r' not in link and 'libGUI' not in link
for name in ['material-cli1','rate-cli1']:
 cli=json.loads((raw/name/'manifest.json').read_text());assert cli['status']=='PASS' and len(cli['cases'])==10
native=json.loads((raw/'native-material-input-binding.json').read_text());assert native['standalone_work']==76886 and native['records']==2218
for p,h in native['sha256'].items():assert sha(raw/p)==h
sidecar=json.loads((raw/'native-final-current/native-full-stop.bytes.json').read_text());assert sidecar['sha256']==sha(raw/'native-final-current/native-full-stop.candidate.txt')
assert sidecar['bytes']==128890 and sidecar['records']==2218 and sidecar['native_B09_exit']=='UNKNOWN' and sidecar['export']=='BLOCK'
for name in ['native-full-stop.candidate.txt','native-material.json']:assert sha(raw/'native-final'/name)==sha(raw/'native-final-current'/name)
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
failed=[c for c in commands if c['exit_code']!=0];assert len(failed)==4,len(failed)
assert {c['evidence'] for c in failed}=={'red-build.txt.json','late-rounding-red.txt.json','material-cli-red.txt.json','headless-link-command.txt.json'}
ci=json.loads((raw/'ci-parent-runs.json').read_text())
m={'schema':1,'stage':stage,'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT','commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B12-final-material.md',
'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'final_build':'build6.txt.json','final_build_identity':format_audit,'suites':suites,'ctest_final':{'tests':383,'failures':0,'skipped':0,'gate_elapsed_seconds':95.811},'OFF_ZAA_final':comparison,'headless_final':'material-cli1/manifest.json and rate-cli1/manifest.json each 10 cases; native-cli2.txt all native rows. Diagnostic component exits only; every report job UNKNOWN/export false',
'runtime_contracts':{'linear_motion_plan':1,'linear_candidate':1,'linear_rates':1,'linear_material':1},'persisted_contracts':'ORIGINAL_IR_MATERIAL_SCENE_PROFILE_3MF_LEDGER_FINGERPRINTS_UNCHANGED_NO_CACHE_NEW_DIAGNOSTIC_ONLY',
'invariant':'OWNED_EXACT_FINAL_DECIMAL_BYTES_SOURCE_EVENT_IDS_ORDER_POSES_PER_RECORD_AND_TOTAL_DOSE_BUDGETS_INDEPENDENT_NOMINAL_INVERSE_K_ONCE_DECLARED_DELIVERY_BOUNDS_SECTION_WIDTH_PARAMETERS_AND_ACTUAL_PARTIAL_PREFIX_NO_FUTURE_MATERIAL_NO_PLANNER_COLLISION_SUPPORT_CERTIFICATES',
'model':'SYNTHETIC_UNCONFIRMED_KNOWN_INITIAL_STATE_DECLARED_CONSTANT_FLUX_RECTANGLE_OR_ADMITTED_ROUNDED_SECTION_FIXED_DECLARED_GAPS_AND_DOSE_ERRORS_NOT_ACTUAL_SUPPORT_GAPS_OR_MEASURED_DELIVERY',
'geometry_scope':'NOMINAL_UPPER_BROAD_PHASE_BOXES_ARE_NOT_FILLED_SOLIDS_DELIVERED_WIDTH_PARAMETERS_PRE_EROSION_EMPTY_INNER_FLAG_FINITE_BUTT_CONDITION_ONLY_FULL_LOWER_UPPER_MEMBERSHIP_UNION_COVERAGE_CONTACT_SUPPORT_PENDING',
'work':'ALL_INPUTS_OWNED_BEFORE_CALLBACKS_PRECEDING_RATE_WORK_AND_ORIGINAL_LEDGER_MAPPING_INCLUDED_BOUNDED_BYTES_RECORDS_OPERATIONS_SINGLE_ABSOLUTE_DEADLINE_CANCEL_SOURCE_SCENE_RATE_MATERIAL_POLICY_ROUNDING_GRADUAL_UNDERFLOW_FINAL_PUBLICATION_NO_PARTIAL_SNAPSHOT_COOPERATIVE_NOT_HARD_PROCESS_SANDBOX',
'strict_flags':'LinearRates.cpp and LinearMaterial.cpp -fno-fast-math -ffp-contract=off no PCH; MSVC registered /fp:strict','standalone_link':'independent static library only, Boost and JSON headers, no slicer/GUI libraries',
'software_label':'CONFIGURED_PARENT_6B0FC1F9_NOT_CURRENT_IMPLEMENTATION_SOURCE_AND_BINARY_HASHES_BIND_ACTUAL_TESTED_BUILD_FULL_B13_PROVENANCE_PENDING',
'native':dict(native,bytes=128890,sha256=sidecar['sha256'],candidate_work=37761,travel=126,original_margin_mm=.01,all_head_parts=7,tip_z_mm=0,pressure_dwell='SEPARATE_ANALYTICAL_AND_CLI_FIXTURES_NOT_NATIVE_SOURCE',actual_partial_prefix='ACTUAL_LAST_DEPOSITION_HALF_G1_PROGRESS_ORIGINAL_MODEL_PARAMETERS_ASSERTED',dose_error='DECLARED_SYNTHETIC_2_PERCENT_NOT_MEASURED'),
'historical_failed_commands':[{k:c[k] for k in ['evidence','exit_code','command']} for c in failed],
'historical_note':'Missing-header scaffold build ran no tests. Late-rounding-red reproduces original adapter final-callback guard defect; fixed common final source guard and positive/negative tests pass. Material-cli-red reproduces new wrong exit0 despite component FAIL; fixed selected component exit passes all mutations. Initial link inventory used default Ninja configuration and found zero Release commands; corrected explicit build-Release.ninja inventory succeeds, not a build/test failure. All history retained, none counted as final evidence.',
'defaults_losses_margins_timeouts_original_negative_tests_unchanged':True,'guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS','review':'SEPARATE_AUTHOR_CRITICAL_REVIEW_INDEPENDENT_PENDING','Linux':{'current_source':'PENDING_NEW_RUN_AFTER_PUSH','observed_utc':ci['observed_utc'],'runs':[{k:r[k] for k in ['id','head_sha','status','conclusion']} for r in ci['response']['workflow_runs']]},'Windows':'NOT_RUN','physical':'NOT_RUN_OPERATOR_PROFILE_UNCONFIRMED',
'not_implemented_or_not_run':'COMPLETE_FINAL_BYTE_LOWER_UPPER_MATERIAL_SOLIDS_UNION_COVERAGE_CONTINUOUS_CONTACT_SUPPORT_VALIDATED_ACTUAL_GAPS_TRANSFORMS_CALIBRATED_DOSE_PRESSURE_ADVANCE_FINAL_FILTERS_PROLOG_END_MOTIONS_COMPLETE_JOB_PLATE_RESOURCE_SOFTWARE_AND_REPORT_HASH_BINDING_FULL_CAP_CONTACT_TOOL_ACCESS_ORDER_ACTUAL_PRINTER_TIME_INDEPENDENT_REVIEW_PLATFORMS_PHYSICAL',
'commands':commands,'raw_sha256':raw_hashes,'raw_to_archive':mapping}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()}
(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for name,h in m['archive_sha256'].items():assert sha(root/name)==h
for name,h in raw_hashes.items():
 path=root/mapping[name];check=hashlib.sha256()
 with (gzip.open(path,'rb') if path.suffix=='.gz' else path.open('rb')) as data:
  for block in iter(lambda:data.read(1024*1024),b''):check.update(block)
 assert check.hexdigest()==h,name
print(json.dumps({'files':len(m['archive_sha256'])+1,'commands':len(commands),'failed_commands':len(failed),'sources':len(files),'dependencies':len(dependencies),'binaries':len(binaries),'raw_byte_mappings':len(mapping),'archive_bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
