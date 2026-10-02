from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();stage='B13-final-report';raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(),'Never overwrite a frozen archive'
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==14
parent=json.loads((root/'docs/nonplanar-dev/evidence/B13-job-context/source-manifest.json').read_text())
dependencies=sorted((set(parent['dependency_sha256'])|set(parent['source_sha256'])|{'src/libslic3r/Nonplanar/VolumePartition.cpp','src/libslic3r/Nonplanar/VolumePartitionExact.cpp','deps_src/admesh/stl.h'})-set(files))
binaries=list(parent['binary_sha256']);assert len(binaries)==4 and all((root/p).is_file() for p in files+dependencies+binaries)
suites={}
for p in sorted(raw.glob('*.xml')):
 s=E.parse(p).getroot().find('testsuite');suites[p.stem]={'cases':len(s.findall('testcase')),'assertions':int(s.get('tests')),'failures':int(s.get('failures')),'skipped':int(s.get('skipped'))}
assert suites['report-focus-final2']=={'cases':16,'assertions':534,'failures':0,'skipped':0}
assert suites['native-final2']=={'cases':1,'assertions':137806,'failures':0,'skipped':0}
ctest=E.parse(raw/'ctest-final2/results.xml').getroot();assert ctest.get('tests')=='414' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest-final2/discovery.json').read_text())['tests'])==414
comparison=json.loads((raw/'baseline-compare-final2.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
app='build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer'
base=json.loads((root/'build/nonplanar-evidence/B13-final-report-final-baselines/manifest.json').read_text());assert base['binary_sha256']==sha(root/app)
assert sha(root/'build/arm64/src/nonplanar_verify/Release/nonplanar_rate_audit')==parent['binary_sha256']['build/arm64/src/nonplanar_verify/Release/nonplanar_rate_audit']
for name in ['build-final2','report-focus-final2','native-final2','ctest-final2','oracle-mutations-final3','provenance-audit-final2','baselines-final2','baseline-compare-final2','package-final','source-audit-final','workflow-parse','authored-diff-check-final','strict-artifact','headless-link-command','compiler','job-context-oracle-final2','job-artifact-oracle-final2']+[f'report-oracle-final2-{i}' for i in range(4)]+[f'{name}-cli-final' for name in ['rate','material','cover','joined','nominal','support']]:
 assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
for name,count in [('rate',10),('material',10),('cover',18),('joined',21),('nominal',21),('support',20)]:
 d=json.loads((raw/(name+'-cli-final')/'manifest.json').read_text());assert d['status']=='PASS' and len(d['cases'])==count
command=next(s for s in (raw/'strict-artifact.txt').read_text().splitlines() if 'JobArtifact.cpp.o -c ' in s)
assert '-fno-fast-math' in command and '-ffp-contract=off' in command and '-include' not in command and '04a8e410' in command
link=(raw/'headless-link-command.txt').read_text().splitlines()[-1];assert 'nonplanar_rate_audit' in link and 'libslic3r' not in link and 'libGUI' not in link
mutations=json.loads((raw/'oracle-mutations-final3/manifest.json').read_text());assert mutations['status']=='PASS' and mutations['positives']==4 and mutations['refusals']==34
provenance=json.loads((raw/'native-provenance-difference-final2.json').read_text());assert provenance['status']=='DIFFERENCE_LOCALIZED_STRICT_IDENTITY_REFUSAL_RETAINED'
assert provenance['vertices']==22 and provenance['oriented_triangles']==40 and provenance['actual_vertices_and_oriented_triangles_same_after_explicit_remap']
assert all(provenance['contexts']['native-final2']['parent_file_identity'].values())
native=json.loads((raw/'native-final2/native-job-report.json').read_text());report=json.loads(native['canonical'])
assert native['records']==2218 and native['evaluations']==145632 and native['candidate_sha256']=='d31bc1ef4ec3c4c1342d0ec2ecd89818749acf52a33ce77a1f149d32f32220a8'
assert len(native['candidate_bytes'].encode())==128890 and len(report['validation']['checks'])==17 and report['validation']['export_decision']=='BLOCK'
assert sum(c['status']=='PASS' and c['execution']=='RUN' for c in report['validation']['checks'])==4
assert sum(c['status']=='UNKNOWN' and c['execution']=='NOT_RUN' for c in report['validation']['checks'])==13
cli_reports={}
for name,status,code,work in [('support','PASS',0,651623),('nominal','PASS',0,300552),('interior','PASS',0,300784),('front','FAIL',2,301060),('per-event','FAIL',2,224712)]:
 d=json.loads((raw/f'native-cli-{name}.txt').read_text());assert d['component_status']==status and d['job_status']=='UNKNOWN' and d['export_allowed'] is False and d['work']==work
 assert json.loads((raw/f'native-cli-{name}.txt.json').read_text())['exit_code']==code;cli_reports[name]=d
ci=json.loads((raw/'ci-before-push.json').read_text());commands=[];raw_hashes={};mapping={};dest.mkdir(parents=True)
for source,prefix in [(raw,Path()),(root/'build/nonplanar-evidence/B13-final-report-baselines',Path('baselines-initial')),(root/'build/nonplanar-evidence/B13-final-report-final-baselines',Path('baselines-final'))]:
 for path in sorted(source.rglob('*')):
  if not path.is_file():continue
  rel=prefix/path.relative_to(source);name=str(path.relative_to(root));raw_hashes[name]=sha(path)
  if path.suffix=='.json':
   try:record=json.loads(path.read_text())
   except json.JSONDecodeError:record=None
   if isinstance(record,dict) and record.get('status')=='COMPLETED' and 'command' in record and 'exit_code' in record:commands.append(dict(evidence=str(rel),**record))
  if path.suffix=='.log':rel=rel.with_suffix('.txt')
  compressed=path.suffix=='.gcode' or path.stat().st_size>250000
  if compressed:rel=Path(str(rel)+'.gz')
  target=dest/rel;assert not target.exists();target.parent.mkdir(parents=True,exist_ok=True)
  if compressed:
   with path.open('rb') as src,target.open('wb') as dst,gzip.GzipFile(filename='',mode='wb',fileobj=dst,mtime=0) as gz:shutil.copyfileobj(src,gz,1048576)
  else:shutil.copyfile(path,target)
  mapping[name]=str(target.relative_to(root))
(dest/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
nonzero=[c for c in commands if c['exit_code']!=0]
m={'schema':1,'stage':stage,'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT','commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B13-final-report.md',
 'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),
 'final_build':'build-final2.txt.json','final_focus':'report-focus-final2.xml','final_native':'native-final2.xml','suites':suites,'ctest_final':{'tests':414,'failures':0,'skipped':0,'elapsed_seconds':96.716,'evidence':'ctest-final2/results.xml','selection':'EXACT_DISCOVERY_INDICES_WITH_JOB_AND_FULL_NATIVE_REPORT_FIXTURES'},'OFF_ZAA_final':comparison,
 'runtime_contracts':{'guarded_job':1,'candidate_binding':1,'report':1,'mandatory_registry':1,'existing_formats':'UNCHANGED'},
 'invariant':'PROTECTED_OWNED_WORKER_ACTUAL_MANIFEST_BYTES_POLICIES_JOURNAL_INITIAL_POSE_INDEPENDENT_RATE_AND_MATERIAL_REPLAY_CUMULATIVE_WORK_ONE_ROOT_DEADLINE_EXCEPTION_PTR_LATCH_FIXED17_MANDATORY_REGISTRY_FOUR_DECLARED_COMPONENTS_THIRTEEN_UNKNOWN_NOT_RUN_BLOCKED_HOST_ADMISSION_ONLY',
 'qualified_scope':'DECLARED_LINEAR_SIMULATION_BYTE_MANIFEST_REPLAY_ONLY_NOT_FULL_NATIVE_SOURCE_PLAN_PROFILE_SCENE_MATERIAL_FIRMWARE_NUMERIC_SOFTWARE_GEOMETRY_SUPPORT_ROUTES_VOLUME_FILTERS_OR_EXPORT',
 'native':{'records':2218,'depositions':2092,'bytes':128890,'sha256':native['candidate_sha256'],'API_assertions':137806,'API_support_work':691602,'API_support_cells':104,'report_work':145632,'report_mandatory':17,'report_pass_run':4,'report_unknown_not_run':13,'job_status':'UNKNOWN','export':'BLOCK','B09_exit':'UNKNOWN','cli':cli_reports,'provenance':provenance},
 'report_oracles':{'positive':4,'refusals':34,'scope':'INDEPENDENT_EXACT_IDENTITY_AND_REGISTRY_CURRENT_DRAFT_SHAPE_ONLY_STANDARD_LIBRARY_NOT_MOTION_MATHEMATICS_GEOMETRY_AUTHENTICITY_OR_PUBLICATION'},
 'headless':'100_ORIGINAL_CLI_CASES_AND_FRESH_NATIVE_ORIGINAL_SUPPORT_NOMINAL_INTERIOR_PASS_FRONT_PER_EVENT_EXPECTED_FAIL_BINARY_UNCHANGED_BY_JOBARTIFACT',
 'strict_flags':'JobArtifact.cpp -fno-fast-math -ffp-contract=off no PCH; registered MSVC strict flags not platform-executed','software_label':'CONFIGURED_PARENT04A8E410_EXACT_CURRENT_SOURCE_DEPENDENCY_BINARY_HASHES_NO_QUALIFIED_SOFTWARE_RESOLVER',
 'review':'SEPARATE_AUTHOR_CRITICAL_AUDIT_INDEPENDENT_PENDING','defaults_losses_margins_original_budgets_negatives_golden_outputs_unchanged':True,'guarded_export':'BLOCK','full_B13':'IN_PROGRESS_THIRTEEN_MANDATORY_DOMAINS_UNIMPLEMENTED','full_B01_B15':'IN_PROGRESS',
 'Linux':{'new_source':'PENDING_EXACT_NEW_RUN_AFTER_PUSH','observed_utc':ci['observed_utc'],'runs':[{k:d[k] for k in ['id','head_sha','status','conclusion']} for d in ci['response']['workflow_runs']]},'Windows':'NOT_RUN','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_UNCONFIRMED_NO_PRESET_PRINTER_OR_CLOUD_EDITS',
 'remaining':'QUALIFIED_SOURCE_PLAN_GUI_PLATE_RESOURCES_SOFTWARE_FULL_GEOMETRY_SUPPORT_ROUTE_ORDER_COMPATIBILITY_FILTERS_PROFILE_SCENE_PRECONDITIONS_DELIVERY_NUMERIC_TARGET_VOLUME_SEAM_VERIFIED_ATOMIC_PUBLICATION_RECOVERY_COPIED_BYTES_EVERY_EXPORT_ROUTE_FULL_B12_B14_B15_HARD_CONTAINMENT_INDEPENDENT_REVIEW_WINDOWS_PHYSICAL',
 'historical':'Earlier six-case report/trace runs and missing-jsonschema oracle errors retained. Standard-library oracle follows. Author audit empty-message latch corrected to exception_ptr, then final2 build/focus/native/CTest/baselines and final3 oracle mutations execute final source. Indexed body16/17 representation difference localized; exact identity refusals retained, no runtime normalization or platform qualification.',
 'archive_command':['python3',str((raw/'archive.py').relative_to(root))],'commands':commands,'nonzero_commands':nonzero,'raw_sha256':raw_hashes,'raw_to_archive':mapping}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p,h in m['archive_sha256'].items():assert sha(root/p)==h
for p,h in raw_hashes.items():
 archive=root/mapping[p];digest=hashlib.sha256()
 with (gzip.open(archive,'rb') if archive.suffix=='.gz' else archive.open('rb')) as f:
  for b in iter(lambda:f.read(1048576),b''):digest.update(b)
 assert digest.hexdigest()==h,p
for p,h in m['source_sha256'].items():assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==h,p
print(json.dumps({'status':'PASS_FROZEN_AND_LOSSLESS_ROUNDTRIP','files':len(m['archive_sha256'])+1,'commands':len(commands),'sources':len(files),'dependencies':len(dependencies),'raw_mappings':len(mapping),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
