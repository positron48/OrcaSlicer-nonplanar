from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();stage='B12-final-solids';raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(),'Never overwrite an existing stage archive'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 return h.hexdigest()
files=['.github/workflows/nonplanar-native.yml','src/nonplanar_verify/LinearMaterial.cpp','src/nonplanar_verify/LinearMaterial.hpp','src/nonplanar_verify/audit.cpp','tests/nonplanar/test_final_material.cpp','tests/nonplanar/test_policy.cpp','scripts/nonplanar/validate_material_cover_cli.py','docs/nonplanar-dev/adr-0072-final-material-solids.md','docs/nonplanar-dev/B12-final-solids.md','docs/nonplanar-dev/B12-final-solids-review.md','docs/nonplanar-dev/README.md','docs/nonplanar-dev/status.json','docs/nonplanar-dev/gate-b-plan.md','docs/nonplanar-dev/next-tasks.md']
parent=json.loads((root/'docs/nonplanar-dev/evidence/B12-final-material/source-manifest.json').read_text())
dependencies=sorted(set(parent['dependency_sha256'])|{p for p in parent['source_sha256'] if p.startswith(('src/','tests/','scripts/'))})
dependencies=[p for p in dependencies if p not in files]+['docs/nonplanar/AGENTS.md','docs/nonplanar/SPEC.md','docs/nonplanar/DATA_CONTRACTS.md']
binaries=list(parent['binary_sha256']);assert len(files)==14 and len(set(files+dependencies))==len(files+dependencies)
assert all((root/p).is_file() for p in files+dependencies+binaries)
suites={}
for p in sorted(raw.glob('*.xml')):
 s=E.parse(p).getroot().find('testsuite');suites[p.stem]={'cases':len(s.findall('testcase')) if s is not None else 0,'assertions':int(s.get('tests')) if s is not None else 0,'failures':int(s.get('failures')) if s is not None else 0,'skipped':int(s.get('skipped')) if s is not None else 0}
for name,cases,assertions in [('focus-final',29,1940),('native-final',1,75158),('solids3',5,159)]:assert suites[name]=={'cases':cases,'assertions':assertions,'failures':0,'skipped':0},name
ctest=E.parse(raw/'ctest-final/results.xml').getroot();assert ctest.get('tests')=='388' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=json.loads((root/'build/nonplanar-evidence'/f'{stage}-baselines'/'manifest.json').read_text());assert base['binary_sha256']==sha(root/binaries[0])
for name in ['build7','focus-final','native-final','ctest-final','cover-cli-final','material-cli-final','rate-cli-final','native-cli-nominal','baseline-capture-final','baseline-compare-final','package-final','source-audit-final','strict-command','headless-link-command','compiler','diff-check-final']:assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
for log,source in [('strict-command.txt','LinearMaterial.cpp'),('headless-link-command.txt','LinearRates.cpp')]:
 strict=[s for s in (raw/log).read_text().splitlines() if ' -c ' in s and source in s][-1];assert '-fno-fast-math' in strict and '-ffp-contract=off' in strict and '-include' not in strict
link=(raw/'headless-link-command.txt').read_text().splitlines()[-1];assert 'nonplanar_rate_audit' in link and 'libslic3r' not in link and 'libGUI' not in link
for name,count in [('cover-cli-final',18),('material-cli-final',10),('rate-cli-final',10)]:
 d=json.loads((raw/name/'manifest.json').read_text());assert d['status']=='PASS' and len(d['cases'])==count
native=json.loads((raw/'native-solid-input-binding.json').read_text());assert native['records']==2218 and native['depositions']==2092
for p,h in native['sha256'].items():assert sha(raw/p)==h
reports={}
for name,status,exitcode,work in [('nominal','PASS',0,130924),('lower','FAIL',2,130214)]:
 report=json.loads((raw/f'native-cli-{name}.txt').read_text());assert report['component_status']==status and report['job_status']=='UNKNOWN' and report['export_allowed'] is False and report['work']==work
 assert json.loads((raw/f'native-cli-{name}.txt.json').read_text())['exit_code']==exitcode;reports[name]=report
assert reports['nominal']['cover_leaves']==1 and reports['lower']['uncovered']
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
   with path.open('rb') as src,target.open('wb') as dst,gzip.GzipFile(filename='',mode='wb',fileobj=dst,mtime=0) as gz:shutil.copyfileobj(src,gz,1048576)
  else:shutil.copyfile(path,target)
  mapping[name]=str(target.relative_to(root))
(dest/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
nonzero=[c for c in commands if c['exit_code']!=0];assert {c['evidence'] for c in nonzero}=={'scaffold-build.txt.json','solids1.txt.json','cover-cli-red.txt.json','native1.txt.json','native-cli-lower.txt.json'}
ci=json.loads((raw/'ci-final-observation.json').read_text())
m={'schema':1,'stage':stage,'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT','commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B12-final-solids.md',
'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'final_build':'build7.txt.json','suites':suites,'ctest_final':{'tests':388,'failures':0,'skipped':0,'gate_elapsed_seconds':95.301},'OFF_ZAA_final':comparison,'headless_final':'18 cover + 10 material + 10 rate cases. Native nominal exit0/PASS and lower expected exit2/FAIL. Every diagnostic job UNKNOWN/export false.',
'runtime_contracts':{'linear_rates':1,'linear_material':1,'linear_material_cover_query':1},'persisted_contracts':'ORIGINAL_IR_MATERIAL_SCENE_PROFILE_3MF_LEDGER_FINGERPRINTS_UNCHANGED_NO_CACHE_NEW_DIAGNOSTIC_ONLY',
'invariant':'OWNED_ACTUAL_FINAL_BYTE_PREFIX_EXACT_ANALYTIC_WHOLE_REGION_RECTANGLE_STADIUM_SOLIDS_SEPARATE_NOMINAL_MAX_DOSE_UPPER_MIN_DOSE_LOWER_ORIGINAL_LOCAL_XY_Z_KERNEL_GROWTH_EROSION_FINITE_BUTTS_PROTECTED_COMPLETE_UNION_PARTITION_OWNER_PROOFS_OR_UNCOVERED_REGION_NO_AABB_FILL_NO_VERTEX_SAMPLING_POSITIVE_PROOF',
'model':'SYNTHETIC_UNCONFIRMED_DECLARED_CONSTANT_XY_FLUX_AFFINE_TOP_GAP_FIXED_LINEAR_LOCAL_XY_Z_GEOMETRY_UNCERTAINTY_SEPARATE_DOSE_ERRORS_NO_PHYSICAL_CALIBRATION',
'lower_semantics':'UNION_OF_INDIVIDUALLY_ERODED_EVENTS_NO_IMPLIED_JOINED_PACKET_SEAM_OUTSIDE_LOWER_IS_ABSENCE_OF_GUARANTEED_COVER_NOT_PHYSICAL_VOID',
'work':'PREFIX_CUMULATIVE_WORK_SINGLE_COVER_DEADLINE_INPUTS_LIMITS_CAPTURED_MAX_EVENTS_BYTES_OPERATIONS_CELLS_DEPTH_STALE_SOURCE_RATE_MATERIAL_CANCEL_ROUNDING_UNDERFLOW_GUARDS_POSITIVE_AND_NEGATIVE_FINAL_PUBLICATION_NO_PARTIAL_CERTIFICATE_NEW_CLI_STAGES_SHARE_ONE_ORIGINAL_SECOND_COOPERATIVE_NOT_HARD_PROCESS_SANDBOX',
'strict_flags':'LinearMaterial.cpp and LinearRates.cpp -fno-fast-math -ffp-contract=off no PCH; MSVC registered /fp:strict','standalone_link':'independent static library only, Boost and JSON headers, no slicer/GUI libraries','software_label':'CONFIGURED_6B0FC1F9_OLDER_THAN_PARENT_380967DD_NOT_CURRENT_IMPLEMENTATION_SOURCE_AND_BINARY_HASHES_BIND_BUILD_FULL_B13_PROVENANCE_PENDING',
'native':dict(native,bytes=128890,sha256='d31bc1ef4ec3c4c1342d0ec2ecd89818749acf52a33ce77a1f149d32f32220a8',travel=126,nominal_work=130924,lower_work=130214,nominal_seconds=.405,lower_seconds=.398,nominal_owner=2218,query_prefix='2217 COMPLETED + ACTUAL LAST DEPOSITION HALF_G1_PROGRESS',original_margin_mm=.01,all_head_parts=7,tip_z_mm=0,last_lower_butt='EMPTY_RETAINED_LOSS_ERROR',B09_exit='UNKNOWN_NO_ROUTE_SNAPSHOT',reports=reports),
'nonzero_commands':[{k:c[k] for k in ['evidence','exit_code','command']} for c in nonzero],
'historical_note':'Scaffold missing-symbol link exits1 before implementation, no tests run. Solids1 exit42 reproduces depth-first boundary UNKNOWN before available gap; breadth-first fixes. Cover-cli-red exit1 reproduces prior report overwriting cover component; final selected report fixed. Native1 exit42 is missing optional evidence directory after geometry assertions pass; native2/final are complete successful executions. Native-cli-lower exit2 is EXPECTED_DIAGNOSTIC_GEOMETRIC_REFUSAL, not failed verification harness. Earlier solids3 5/159 predates seven added late-guard assertions; final combined 29/1940 contains final five solid cases. All original negative cases/margins/defaults remain.',
'defaults_losses_margins_timeouts_original_negative_tests_unchanged':True,'guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS','review':'SEPARATE_AUTHOR_CRITICAL_REVIEW_INDEPENDENT_PENDING','Linux':{'current_source':'PENDING_NEW_RUN_AFTER_PUSH','observed_utc':ci['observed_utc'],'runs':[{k:r[k] for k in ['id','head_sha','status','conclusion']} for r in ci['response']['workflow_runs']]},'Windows':'NOT_RUN','physical':'NOT_RUN_OPERATOR_PROFILE_UNCONFIRMED',
'not_implemented_or_not_run':'INDEPENDENT_JOINED_PACKET_CONTINUITY_UNCERTAINTY_FULL_CAP_COVERAGE_ACTUAL_GAP_HEAD_CONTACT_COMPLETE_ROUTES_UNION_VOLUME_QUALIFIED_DOSE_FLOW_TRANSFORMS_PROLOG_END_FILTERS_JOB_PLATE_RESOURCES_SOFTWARE_FINAL_BYTE_REPORT_BINDING_EXPORT_UI_HARD_CONTAINMENT_INDEPENDENT_REVIEW_PLATFORMS_PHYSICAL',
'commands':commands,'raw_sha256':raw_hashes,'raw_to_archive':mapping}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p,h in m['archive_sha256'].items():assert sha(root/p)==h
for p,h in raw_hashes.items():
 archive=root/mapping[p];a=hashlib.sha256()
 with (gzip.open(archive,'rb') if archive.suffix=='.gz' else archive.open('rb')) as f:
  for b in iter(lambda:f.read(1048576),b''):a.update(b)
 assert a.hexdigest()==h,p
print(json.dumps({'files':len(m['archive_sha256'])+1,'commands':len(commands),'historical_failure_commands':4,'expected_diagnostic_refusals':1,'sources':len(files),'dependencies':len(dependencies),'binaries':len(binaries),'raw_byte_mappings':len(mapping),'archive_bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
