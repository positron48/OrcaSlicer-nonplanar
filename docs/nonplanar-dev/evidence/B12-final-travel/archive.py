from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();stage='B12-final-travel';raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==16
parent=json.loads((root/'docs/nonplanar-dev/evidence/B13-native-departure/source-manifest.json').read_text())
dependencies=sorted((set(parent['dependency_sha256'])|set(parent['source_sha256']))-set(files));binaries=list(parent['binary_sha256'])
assert len(binaries)==4 and all((root/p).is_file() for p in files+dependencies+binaries)
qualified=['build-final','focus-final','ctest-final2','native-cli-final','baseline-capture-final','baseline-compare-final',
 'rate-cli-final2','material-cli-final2','cover-cli-final2','joined-cli-final2','nominal-cli-final2','support-cli-final2','travel-cli-final2',
 'provenance-final','job-context-final','job-artifact-final','strict-final','compiler','package-final','source-audit-final','authored-diff-check']
qualified+=['candidate-report-oracle','candidate-report-invalid-material-oracle','candidate-report-failed-material-oracle','native-lineage-report-oracle','native-departure-report-oracle']
for name in qualified:assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
for p in files:assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==sha(root/p),p
suites={}
for p in raw.glob('*.xml'):
 s=E.parse(p).getroot().find('testsuite');suites[p.stem]={'cases':len(s.findall('testcase')),'assertions':int(s.get('tests')),'failures':int(s.get('failures')),'skipped':int(s.get('skipped'))}
assert suites['focus-final']=={'cases':7,'assertions':36076,'failures':0,'skipped':0}
ctest=E.parse(raw/'ctest-final2/results.xml').getroot();assert ctest.get('tests')=='432' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest-final2/discovery.json').read_text())['tests'])==432
for case in ctest.findall('testcase'):
 assert case.get('status')=='run' and all(case.find(tag) is None for tag in ('skipped','failure','error'))
native=next(t for t in ctest.findall('testcase') if 'independent final decimal geometry' in t.get('name',''))
assert '34703 assertions' in native.find('system-out').text
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=root/'build/nonplanar-evidence/B12-final-travel-final-baselines';assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
provenance=json.loads((raw/'provenance-final.json').read_text());assert provenance['differences']==[] and len(provenance['original_files'])==19
assert provenance['native']['candidate_sha256']=='b8eb15681127c1a8db57ce0aa257326763f16832b82fdafd13ed091c4bd98ef8'
cli={}
for name in ['rate','material','cover','joined','nominal','support','travel']:
 j=json.loads((raw/(name+'-cli-final2')/'manifest.json').read_text());assert j['status']=='PASS'
 cli[name]=len(j['cases']);assert all(c['inputs_unchanged'] for c in j['cases'])
assert sum(v for k,v in cli.items() if k!='travel')==100 and cli['travel']==40
report=json.loads((raw/'native-cli-final.txt').read_text());assert report['component_status']=='PASS' and report['job_status']=='UNKNOWN' and report['export_allowed'] is False
assert report['cells']==133 and report['leaves']==77 and report['work']==419381
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
m={'schema':1,'stage':stage,'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT',
 'commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B12-final-travel.md',
 'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},
 'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'suites':suites,'final_build':'build-final.txt.json','final_focus':'focus-final.xml',
 'ctest_final':{'tests':432,'failures':0,'skipped':0,'elapsed_seconds':json.loads((raw/'ctest-final2.txt.json').read_text())['elapsed_seconds'],
 'evidence':'ctest-final2/results.xml','selection':'EXACT_ORIGINAL_NONPLANAR_FFF_DISCOVERY_INDICES_PLUS_SEVEN_TRAVEL_CASES'},
 'OFF_ZAA_final':comparison,'runtime_contracts':{'travel_query':1,'production_projects_profiles_and_previous_formats':'UNCHANGED'},
 'invariant':'MAXIMAL_COMPLETE_FINAL_DECIMAL_TRAVEL_BLOCK_ALL_FIXED_HEAD_STATIC_PREVIOUS_ACTUAL_UPPER_WHOLE_CELL_EXCLUSION_ORIGINAL_MARGINS_NO_SAMPLED_PASS',
 'qualified_scope':'DECLARED_FIXED_AXIS_SYNTHETIC_TRAVEL_COMPONENT_ONLY_NO_DEPOSITION_CONTACT_WHOLE_JOB_EXPORT_OR_PHYSICAL_QUALIFICATION',
 'native':provenance['native'],'original_outputs':provenance,'native_CLI':{'report':'native-cli-final.txt.gz','elapsed_seconds':json.loads((raw/'native-cli-final.txt.json').read_text())['elapsed_seconds'],'absolute_deadline_ms':1000},
 'independent_oracle':'113_BIT_FINAL_TEXT_ALL_WHOLE_LEAF_HEAD_STATIC_PREVIOUS_BEAD_EQUATIONS_FULL_LOCAL_TIME_DOMAIN_MEASURE_DISJOINT_PARTITIONS',
 'CLI_cases':cli,'strict_flags':'LinearMaterial.cpp no PCH -fno-fast-math -ffp-contract=off; MSVC registration unchanged NOT_RUN',
 'software_label':'CONFIGURED_PARENT9A3ED1CA_EXACT_CURRENT_SOURCE_DEPENDENCY_BINARY_HASHES_NO_QUALIFIED_RESOLVER',
 'review':'SEPARATE_AUTHOR_CRITICAL_AUDIT_INDEPENDENT_PENDING','guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS',
 'mandatory_report':{'checks':17,'pass_run':4,'unknown_not_run':13},'Linux':{'new_source':'PENDING_AFTER_PUSH','parent':json.loads((raw/'ci-parent.json').read_text())},
 'Windows':'NOT_RUN','GUI':'NOT_RUN_FULL_PIPELINE','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_UNCONFIRMED_NO_PRESET_PRINTER_OR_CLOUD_EDITS',
 'historical':'Initial typed test mapper/writer compile failures retained. Native CLI first returns UNKNOWN at absolute1s root; individual complete disjoint Upper-box pruning permits PASS at original margins/limits. Initial CTest3 fails solely absent precreated job evidence directory; fresh full432 rerun passes. No production/test relaxation or evidence overwrite.',
 'archive_command':['python3',str((raw/'archive.py').relative_to(root))],'commands':commands,'nonzero_commands':[c for c in commands if c['exit_code']!=0],
 'excluded_commands_from_final_claim':['build2.txt.json','build4.txt.json','native-cli1.txt.json','ctest-final.txt.json'],
 'raw_sha256':raw_hashes,'raw_to_archive':mapping,'transport':'GZIP_LOG_TXT_GCODE_AND_LARGE_FILES_EXACT_RAW_BYTES_NO_FORCE_ADD_NO_WHITESPACE_TRIMMING'}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p,h in m['archive_sha256'].items():assert sha(root/p)==h
for p,h in raw_hashes.items():
 archive=root/mapping[p];digest=hashlib.sha256()
 with (gzip.open(archive,'rb') if archive.suffix=='.gz' else archive.open('rb')) as f:
  for chunk in iter(lambda:f.read(1048576),b''):digest.update(chunk)
 assert digest.hexdigest()==h,p
print(json.dumps({'status':'PASS_FROZEN_LOSSLESS_EVIDENCE','files':len(m['archive_sha256'])+1,'commands':len(commands),'sources':len(files),'dependencies':len(dependencies),'raw_mappings':len(mapping),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
