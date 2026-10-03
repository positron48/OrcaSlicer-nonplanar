from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();stage='B12-supported-deposition';raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==17
parent=json.loads((root/'docs/nonplanar-dev/evidence/B12-forming-polyline/source-manifest.json').read_text())
dependencies=sorted((set(parent['dependency_sha256'])|set(parent['source_sha256']))-set(files));binaries=list(parent['binary_sha256'])
assert len(binaries)==4 and all((root/p).is_file() for p in files+dependencies+binaries)
qualified=['build-qualified','focus-qualified','ctest-qualified','native-polyline-cli-final','native-contour-cli-final','native-contact-cli-final','native-deposition-cli-final','native-travel-cli-final','baseline-capture-final','baseline-compare-final','rate-cli-final','material-cli-final','cover-cli-final','joined-cli-final','nominal-cli-final','support-cli-final','travel-cli-final','deposition-cli-final','contact-cli-final','polyline-cli-final','provenance-classified','job-context-final','job-artifact-final','native-report-oracle','strict-final','compiler','package-final','source-audit-final','authored-diff-check','workflow-qualified','candidate-report-oracle','candidate-report-invalid-material-oracle','candidate-report-failed-material-oracle','native-lineage-report-oracle','native-departure-report-oracle','supported-cli-final','native-supported-cli-final','cli-regressions-classified']
for name in qualified:assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
for p in files:assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==sha(root/p),p
suites={}
for p in raw.glob('*.xml'):
 s=E.parse(p).getroot().find('testsuite');suites[p.stem]={'cases':len(s.findall('testcase')),'assertions':int(s.get('tests')),'failures':int(s.get('failures')),'skipped':int(s.get('skipped'))}
assert suites['focus-qualified']=={'cases':30,'assertions':2433417,'failures':0,'skipped':0}
ctest=E.parse(raw/'ctest-qualified/results.xml').getroot();assert ctest.get('tests')=='449' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest-qualified/discovery.json').read_text())['tests'])==449
for case in ctest.findall('testcase'):assert case.get('status')=='run' and all(case.find(tag) is None for tag in ('skipped','failure','error'))
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=root/'build/nonplanar-evidence/B12-supported-deposition-final-baselines';assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
provenance=json.loads((raw/'provenance-classified.json').read_text());assert len(provenance['original_files'])==19 and provenance['source_drift']['unchanged_original_files']==12 and len(provenance['differences'])==7
assert [len(provenance[k]) for k in ['old_travel_exact','old_deposition_exact','old_contact_exact']]==[4,4,6]
cli={}
for name in ['rate','material','cover','joined','nominal','support','travel','deposition','contact','polyline','supported']:
 j=json.loads((raw/(('supported' if name=='supported' else name)+'-cli-final')/'manifest.json').read_text());assert j['status']=='PASS'
 cli[name]=len(j['cases']);assert all(c['inputs_unchanged'] for c in j['cases'])
assert sum(cli.values())==437 and cli['supported']==105 and cli['contact']==81 and cli['polyline']==90 and cli['travel']==40 and cli['deposition']==21
native_cli={}
for name,cells,leaves,work in [('polyline',28,28,270344),('contact',28,28,270219),('deposition',300,164,708889),('travel',133,77,419381)]:
 stem='native-'+name+'-cli-final';report=json.loads((raw/(stem+'.txt')).read_text())
 assert report['component_status']=='PASS' and report['job_status']=='UNKNOWN' and report['export_allowed'] is False
 assert report['cells']==cells and report['leaves']==leaves and report['work']==work
 if name in ['contact','polyline','supported']:assert report['forming_contact_cells']==4
 native_cli[name]={'report':stem+'.txt.gz','elapsed_seconds':json.loads((raw/(stem+'.txt.json')).read_text())['elapsed_seconds'],'absolute_deadline_ms':1000,'work':work}
report=json.loads((raw/'native-supported-cli-final.txt').read_text())
assert report['component_status']=='PASS' and report['job_status']=='UNKNOWN' and report['export_allowed'] is False
assert report['cells']==48 and report['geometry_cells']==28 and report['geometry_leaves']==28 and report['work']==598131
assert report['support_runs']==1 and report['underlying_completed_records']==2198
native_cli['supported']={'report':'native-supported-cli-final.txt.gz','elapsed_seconds':json.loads((raw/'native-supported-cli-final.txt.json').read_text())['elapsed_seconds'],'absolute_deadline_ms':1000,'work':report['work'],'cells':48}
contour=json.loads((raw/'native-contour-cli-final/run.json').read_text());assert contour['exit_code']==2 and contour['report']['component_status']=='FAIL' and contour['report']['leaves']==0
assert contour['report']['work']==831696 and contour['report']['unproved_cell']['record']==2093 and contour['report']['witness']['material_event']==2077
strict=[s for s in (raw/'strict-final.txt').read_text().splitlines() if 'LinearMaterial.cpp.o -c ' in s]
assert len(strict)==1 and '-fno-fast-math' in strict[0] and '-ffp-contract=off' in strict[0] and '-include' not in strict[0]
commands=[];raw_hashes={};mapping={};dest.mkdir(parents=True)
for source,prefix in [(raw,Path()),(base,Path('baselines-final'))]:
 for path in sorted(source.rglob('*')):
  if not path.is_file() or path.name.startswith('archive-execution.'):continue
  rel=prefix/path.relative_to(source);name=str(path.relative_to(root));raw_hashes[name]=sha(path)
  if path.suffix=='.json':
   try:entry=json.loads(path.read_text())
   except json.JSONDecodeError:entry=None
   if isinstance(entry,dict) and entry.get('status')=='COMPLETED' and 'command' in entry and 'exit_code' in entry:commands.append(dict(evidence=str(rel),**entry))
  compressed=path.suffix in ('.log','.txt','.gcode') or path.stat().st_size>250000
  if compressed:rel=Path(str(rel)+'.gz')
  target=dest/rel;assert not target.exists();target.parent.mkdir(parents=True,exist_ok=True)
  if compressed:
   with path.open('rb') as src,target.open('wb') as dst,gzip.GzipFile(filename='',mode='wb',fileobj=dst,mtime=0) as gz:shutil.copyfileobj(src,gz,1048576)
  else:shutil.copyfile(path,target)
  mapping[name]=str(target.relative_to(root))
(dest/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
m={'schema':1,'stage':stage,'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT','commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B12-supported-deposition.md',
 'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'suites':suites,'final_build':'build-qualified.txt.json','final_focus':'focus-qualified.xml',
 'ctest_final':{'tests':449,'failures':0,'skipped':0,'elapsed_seconds':json.loads((raw/'ctest-qualified.txt.json').read_text())['elapsed_seconds'],'evidence':'ctest-qualified/results.xml','selection':'PREVIOUS_NATIVE_DISCOVERY_PLUS_FOUR_ANALYTICAL_AND_ONE_NATIVE_SUPPORTED_DEPOSITION_CASE'},'OFF_ZAA_final':comparison,
 'runtime_contracts':{'supported_deposition':1,'support_fields':{'root':5,'join':6,'run':3,'support_policy':10},'standalone_support':'UNCHANGED_PRE_RUN_SCOPE','scene_query':1,'diagnostic_contact_versions':[1,2],'contact_fields':{'v1':16,'v2':17},'production_projects_profiles_previous_formats':'UNCHANGED'},
 'invariant':'COMPLETE_MAXIMAL_FINAL_BYTE_DEPOSIT_BLOCK_FULL_HEAD_CONTACT_AND_EVERY_ACTUAL_RUN_UNDERLYING_NOMINAL_VERTICAL_NORMAL_GAPS_JOINED_LOWER_ANCHORS_EXACT_SAME_OWNER_PRE_BLOCK_PREFIX_EXPLICIT_CHILD_SCOPE_SHARED_ROOT_WORK_CELL_DEADLINE_COMPLETED_POLICY_GUARDS_NO_PARTIAL_CERTIFICATE',
 'qualified_scope':'COMPLETE_DECLARED_BLOCK_GEOMETRY_AND_PRE_BLOCK_UNDERLYING_SUPPORT_ONLY_NO_MEASURED_CONTACT_ADJACENT_SEAM_FULL_FILL_WHOLE_JOB_EXPORT_OR_PHYSICAL_QUALIFICATION','native':provenance['native'],'native_supported':provenance['supported'],'CLI_regressions':json.loads((raw/'cli-regressions.json').read_text()),'original_outputs':provenance,'native_CLI':native_cli,'native_contour_CLI':contour,
 'independent_oracle':'113_BIT_FINAL_TEXT_TRUE_PI_FULL_GEOMETRY_AND_COMPLETE_RUN_SUPPORT_RAY_LOWER_NOMINAL_PARTITIONS_PLUS_EXACT_OWNERS_PREFIX_ALL_MAXIMAL_RUNS_POLICIES_CUMULATIVE_CELLS_NO_PRODUCT_QUERY','CLI_cases':cli,'strict_flags':'LinearMaterial.cpp no PCH -fno-fast-math -ffp-contract=off; MSVC NOT_RUN','software_label':'CONFIGURED_PARENT9A3ED1CA_EXACT_CURRENT_SOURCE_DEPENDENCY_BINARY_HASHES_RESOLVER_UNQUALIFIED',
 'review':'SEPARATE_AUTHOR_CRITICAL_AUDIT_INDEPENDENT_PENDING','guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS','mandatory_report':{'checks':17,'pass_run':4,'unknown_not_run':13},'Linux':{'new_source':'PENDING_AFTER_PUSH','parent':json.loads((raw/'ci-parent-final.json').read_text())},'Windows':'NOT_RUN','GUI':'NOT_RUN_FULL_PIPELINE','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_KNOWN_GEOMETRY_UNCONFIRMED_NO_PRESET_PRINTER_CLOUD_EDITS',
 'historical':'Expected-red linker/corner and initial rising-cross-anchor matrix failure retained. Strict source identity and CLI counter comparison refusals retained. Derived-body order/hash diagnosis and three nominal deadline-only work/cell differences do not normalize bytes or reuse proofs. Final qualified tests pass with original margins/limits and negative assertions retained.',
 'archive_command':['python3',str((raw/'archive.py').relative_to(root))],'commands':commands,'nonzero_commands':[c for c in commands if c['exit_code']!=0],'qualified_commands':qualified,'excluded_commands_from_final_claim':[c['evidence'] for c in commands if Path(c['evidence']).name.removesuffix('.txt.json') not in qualified],
 'raw_sha256':raw_hashes,'raw_to_archive':mapping,'transport':'GZIP_LOG_TXT_GCODE_AND_LARGE_FILES_EXACT_RAW_BYTES_NO_FORCE_ADD_NO_WHITESPACE_TRIMMING'}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p,h in m['archive_sha256'].items():assert sha(root/p)==h
for p,h in raw_hashes.items():
 archive=root/mapping[p];digest=hashlib.sha256()
 with (gzip.open(archive,'rb') if archive.suffix=='.gz' else archive.open('rb')) as f:
  for b in iter(lambda:f.read(1048576),b''):digest.update(b)
 assert digest.hexdigest()==h,p
print(json.dumps({'status':'PASS_FROZEN_LOSSLESS_EVIDENCE','files':len(m['archive_sha256'])+1,'commands':len(commands),'sources':len(files),'dependencies':len(dependencies),'raw_mappings':len(mapping),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
