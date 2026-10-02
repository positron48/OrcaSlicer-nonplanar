from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();stage='B13-native-lineage';raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==18
parent=json.loads((root/'docs/nonplanar-dev/evidence/B13-final-report/source-manifest.json').read_text())
dependencies=sorted((set(parent['dependency_sha256'])|set(parent['source_sha256']))-set(files));binaries=list(parent['binary_sha256']);assert len(binaries)==4
assert all((root/p).is_file() for p in files+dependencies+binaries)
suites={}
for p in raw.glob('*.xml'):
 s=E.parse(p).getroot().find('testsuite');suites[p.stem]={'cases':len(s.findall('testcase')),'assertions':int(s.get('tests')),'failures':int(s.get('failures')),'skipped':int(s.get('skipped'))}
assert suites['focus-final2']=={'cases':21,'assertions':666,'failures':0,'skipped':0}
ctest=E.parse(raw/'ctest-final2/results.xml').getroot();assert ctest.get('tests')=='419' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest-final2/discovery.json').read_text())['tests'])==419
for name in ['build-final','focus-final2','ctest-final2','oracle-mutations','baseline-compare-final','cli-gates','job-context-oracle','job-artifact-oracle','ctest-native-lineage-oracle','provenance-audit','package','source-audit','strict-native','headless-link','compiler','workflow-parse','authored-diff-check']:
 assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=root/'build/nonplanar-evidence/B13-native-lineage-final-baselines';assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
mutations=json.loads((raw/'oracle-mutations/manifest.json').read_text());assert mutations['positives']==5 and mutations['refusals']==46
native=json.loads((raw/'final-job-fixtures/native-lineage-report.json').read_text());report=json.loads(native['canonical']);assert native['records']==2197 and native['evaluations']==144333 and len(native['candidate_bytes'])==127589
assert native['candidate_sha256']=='e338ae9a6da37420573361725b6ce3b27691edce4e78de1c37d86e11a14a3739'
assert len(report['validation']['checks'])==17 and report['validation']['export_decision']=='BLOCK'
assert sum(c['status']=='PASS' and c['execution']=='RUN' for c in report['validation']['checks'])==4
assert sum(c['status']=='UNKNOWN' and c['execution']=='NOT_RUN' for c in report['validation']['checks'])==13
provenance=json.loads((raw/'provenance-comparison.json').read_text());assert provenance['differences']==[] and len(provenance['original_files'])==11 and all(v['same_bytes'] for v in provenance['original_files'].values())
command=next(s for s in (raw/'strict-native.txt').read_text().splitlines() if 'JobNative.cpp.o -c ' in s)
assert '-fno-fast-math' in command and '-ffp-contract=off' in command and '-include' not in command and '9a3ed1ca' in command
link=(raw/'headless-link.txt').read_text().splitlines()[-1];assert 'nonplanar_rate_audit' in link and 'libslic3r' not in link and 'libGUI' not in link
for name,count in [('rate',10),('material',10),('cover',18),('joined',21),('nominal',21),('support',20)]:
 d=json.loads((raw/(name+'-cli')/'manifest.json').read_text());assert d['status']=='PASS' and len(d['cases'])==count
commands=[];raw_hashes={};mapping={};dest.mkdir(parents=True)
for source,prefix in [(raw,Path()),(base,Path('baselines-final'))]:
 for path in sorted(source.rglob('*')):
  if not path.is_file():continue
  rel=prefix/path.relative_to(source);name=str(path.relative_to(root));raw_hashes[name]=sha(path)
  if path.suffix=='.json':
   try:record=json.loads(path.read_text())
   except json.JSONDecodeError:record=None
   if isinstance(record,dict) and record.get('status')=='COMPLETED' and 'command' in record and 'exit_code' in record:commands.append(dict(evidence=str(rel),**record))
  compressed=path.suffix=='.gcode' or path.stat().st_size>250000
  if compressed:rel=Path(str(rel)+'.gz')
  target=dest/rel;assert not target.exists();target.parent.mkdir(parents=True,exist_ok=True)
  if compressed:
   with path.open('rb') as src,target.open('wb') as dst,gzip.GzipFile(filename='',mode='wb',fileobj=dst,mtime=0) as gz:shutil.copyfileobj(src,gz,1048576)
  else:shutil.copyfile(path,target)
  mapping[name]=str(target.relative_to(root))
(dest/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
m={'schema':1,'stage':stage,'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT','commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B13-native-lineage.md',
 'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'suites':suites,
 'final_build':'build-final.txt.json','final_focus':'focus-final2.xml','ctest_final':{'tests':419,'failures':0,'skipped':0,'elapsed_seconds':97.213,'evidence':'ctest-final2/results.xml','selection':'EXACT_DISCOVERY_INDICES_WITH_ORIGINAL_AND_OWNED_NATIVE_LINEAGE_REPORTS'},'OFF_ZAA_final':comparison,
 'runtime_contracts':{'guarded_job':1,'candidate_binding':1,'optional_native_candidate_binding':2,'native_body_hatch_plan_lineage':1,'report':1,'mandatory_registry':1,'production_formats':'UNCHANGED'},
 'invariant':'OWNED_EXACT_ORIGINAL_BYTES_PRE_APPLY_SOURCE_PLACEMENT_PARTITION_NATIVE_BODY_EFFECTIVE_CONFIG_MATERIAL_HATCH_SELECTED_COMPLETE_BODY_CAP_JOURNAL_ACTUAL_LINEAR_SOURCE_OUTPUT_EXCEPT_REDUCED_SPEED_ACCELERATION_FINAL_BYTES_PRIVATE_CURRENT_JOB_ATTEMPT_LINEAGE',
 'qualified_scope':'BOUNDED_NATIVE_DEPENDENCY_IDENTITY_ONLY_NOT_COMPLETE_SOURCE_TARGET_GUI_PLATE_GEOMETRY_SUPPORT_CONTACT_ROUTES_COMPATIBILITY_FILTERS_SOFTWARE_RESOURCE_MACHINE_OR_DELIVERY_QUALIFICATION',
 'native':{'original_body_records':2034,'original_body_depositions':1911,'selected_assembly_records':2197,'selected_assembly_depositions':2072,'bytes':127589,'candidate_sha256':native['candidate_sha256'],'report_work':144333,'mandatory':17,'pass_run':4,'unknown_not_run':13,'full_source_model_plan_binding':'UNKNOWN_NOT_RUN','job_status':'UNKNOWN','export':'BLOCK','original2218_fixture':provenance},
 'independent_oracles':{'positive_reports':5,'refusals':46,'scope':'EXACT_ANCESTOR_CANONICAL_HASH_FULL_BODY_PREFIX_AND_ROW_IDENTITY_ONLY_NOT_MOTION_MATH_GEOMETRY_OR_AUTHENTICITY'},'CLI_cases':100,
 'strict_flags':'JobNative.cpp no PCH; -fno-fast-math -ffp-contract=off; MSVC strict registered NOT_RUN','software_label':'CONFIGURED_PARENT9A3ED1CA_EXACT_CURRENT_SOURCE_DEPENDENCY_BINARY_HASHES_NO_QUALIFIED_RESOLVER','review':'SEPARATE_AUTHOR_CRITICAL_AUDIT_INDEPENDENT_PENDING',
 'guarded_export':'BLOCK','full_B13':'IN_PROGRESS_THIRTEEN_MANDATORY_DOMAINS_PENDING','full_B01_B15':'IN_PROGRESS','original_budgets_losses_margins_negatives_goldens_unchanged':True,
 'Linux':{'new_source':'PENDING_AFTER_PUSH','inherited':json.loads((raw/'ci-inherited.json').read_text())},'Windows':'NOT_RUN','GUI':'NOT_RUN_FULL_PIPELINE','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_UNCONFIRMED_NO_PRESET_PRINTER_OR_CLOUD_EDITS',
 'historical':'First focus failed after successful body generation because a test dereferenced omitted API-key; corrected absence assertion. First native Python oracle used payload index instead of actual acceleration index; corrected5/6. Author review then added root deadline/fenv negatives before final build/focus/CTest. Failed intermediate logs retained. LLDB shell argdumper could not launch; Catch XML and source established the test-null error.',
 'archive_command':['python3',str((raw/'archive.py').relative_to(root))],'commands':commands,'nonzero_commands':[c for c in commands if c['exit_code']!=0],'raw_sha256':raw_hashes,'raw_to_archive':mapping}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p,h in m['archive_sha256'].items():assert sha(root/p)==h
for p,h in raw_hashes.items():
 archive=root/mapping[p];digest=hashlib.sha256()
 with (gzip.open(archive,'rb') if archive.suffix=='.gz' else archive.open('rb')) as f:
  for b in iter(lambda:f.read(1048576),b''):digest.update(b)
 assert digest.hexdigest()==h,p
for p,h in m['source_sha256'].items():assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==h,p
print(json.dumps({'status':'PASS_FROZEN_LOSSLESS_EVIDENCE','files':len(m['archive_sha256'])+1,'commands':len(commands),'sources':len(files),'dependencies':len(dependencies),'raw_mappings':len(mapping),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
