from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();stage='B12-final-deposition';raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==15
parent=json.loads((root/'docs/nonplanar-dev/evidence/B12-final-travel/source-manifest.json').read_text())
dependencies=sorted((set(parent['dependency_sha256'])|set(parent['source_sha256']))-set(files));binaries=list(parent['binary_sha256'])
assert len(binaries)==4 and all((root/p).is_file() for p in files+dependencies+binaries)
qualified=['build-final','focus6','ctest-final','native-deposition-cli-final','native-travel-cli-final','baseline-capture-final','baseline-compare-final',
 'rate-cli-final','material-cli-final3','cover-cli-final','joined-cli-final','nominal-cli-final','support-cli-final','travel-cli-final','deposition-cli-final',
 'provenance-final','job-context-final','job-artifact-final','strict-final','compiler','package-final','source-audit-final','authored-diff-check']
qualified+=['candidate-report-oracle','candidate-report-invalid-material-oracle','candidate-report-failed-material-oracle','native-lineage-report-oracle','native-departure-report-oracle']
for name in qualified:assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
for p in files:assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==sha(root/p),p
suites={}
for p in raw.glob('*.xml'):
 s=E.parse(p).getroot().find('testsuite');suites[p.stem]={'cases':len(s.findall('testcase')),'assertions':int(s.get('tests')),'failures':int(s.get('failures')),'skipped':int(s.get('skipped'))}
assert suites['focus6']=={'cases':10,'assertions':74169,'failures':0,'skipped':0}
ctest=E.parse(raw/'ctest-final/results.xml').getroot();assert ctest.get('tests')=='435' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest-final/discovery.json').read_text())['tests'])==435
for case in ctest.findall('testcase'):assert case.get('status')=='run' and all(case.find(tag) is None for tag in ('skipped','failure','error'))
assert sum('independent final deposition geometry' in c.get('name','') for c in ctest.findall('testcase'))==1
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=root/'build/nonplanar-evidence/B12-final-deposition-final-baselines';assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
provenance=json.loads((raw/'provenance-final.json').read_text());assert provenance['differences']==[] and len(provenance['original_files'])==19 and len(provenance['old_travel_exact'])==4
assert provenance['native']['candidate_sha256']=='b8eb15681127c1a8db57ce0aa257326763f16832b82fdafd13ed091c4bd98ef8'
cli={}
for name in ['rate','material','cover','joined','nominal','support','travel','deposition']:
 folder=name+'-cli-final'+('3' if name=='material' else '');j=json.loads((raw/folder/'manifest.json').read_text());assert j['status']=='PASS'
 cli[name]=len(j['cases']);assert all(c['inputs_unchanged'] for c in j['cases'])
assert sum(v for k,v in cli.items() if k not in ('travel','deposition'))==100 and cli['travel']==40 and cli['deposition']==21
native_cli={}
for name,cells,leaves,work in [('deposition',300,164,708889),('travel',133,77,419381)]:
 stem='native-'+name+'-cli-final';report=json.loads((raw/(stem+'.txt')).read_text())
 assert report['component_status']=='PASS' and report['job_status']=='UNKNOWN' and report['export_allowed'] is False
 assert report['cells']==cells and report['leaves']==leaves and report['work']==work
 native_cli[name]={'report':stem+'.txt.gz','elapsed_seconds':json.loads((raw/(stem+'.txt.json')).read_text())['elapsed_seconds'],'absolute_deadline_ms':1000,'work':work}
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
 'commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B12-final-deposition.md',
 'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},
 'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'suites':suites,'final_build':'build-final.txt.json','final_focus':'focus6.xml',
 'ctest_final':{'tests':435,'failures':0,'skipped':0,'elapsed_seconds':json.loads((raw/'ctest-final.txt.json').read_text())['elapsed_seconds'],
 'evidence':'ctest-final/results.xml','selection':'EXACT_ORIGINAL_NONPLANAR_FFF_DISCOVERY_INDICES_PLUS_THREE_DEPOSITION_CASES'},
 'OFF_ZAA_final':comparison,'runtime_contracts':{'scene_query':1,'production_projects_profiles_and_previous_formats':'UNCHANGED'},
 'invariant':'MAXIMAL_COMPLETE_FINAL_DECIMAL_DEPOSIT_BLOCK_ALL_FIXED_RIGID_HEAD_STATIC_EARLIER_SIMULTANEOUS_CURRENT_UPPER_WHOLE_CELL_EXCLUSION_ORIGINAL_MARGINS_NO_SAMPLED_PASS_NO_FUTURE_WITNESS',
 'qualified_scope':'DECLARED_FIXED_AXIS_SYNTHETIC_RIGID_DEPOSIT_COMPONENT_ONLY_NO_CONTACT_SUPPORT_FILL_WHOLE_JOB_EXPORT_OR_PHYSICAL_QUALIFICATION',
 'native':provenance['native'],'original_outputs':provenance,'native_CLI':native_cli,
 'independent_oracle':'113_BIT_FINAL_TEXT_ALL_WHOLE_LEAF_HEAD_STATIC_EARLIER_CURRENT_BEAD_EQUATIONS_FULL_DOMAIN_MEASURE_DISJOINT_PARTITIONS_AND_GROWN_RECTANGLE_WITNESS',
 'CLI_cases':cli,'strict_flags':'LinearMaterial.cpp no PCH -fno-fast-math -ffp-contract=off; MSVC registration unchanged NOT_RUN',
 'software_label':'CONFIGURED_PARENT9A3ED1CA_EXACT_CURRENT_SOURCE_DEPENDENCY_BINARY_HASHES_NO_QUALIFIED_RESOLVER',
 'review':'SEPARATE_AUTHOR_CRITICAL_AUDIT_INDEPENDENT_PENDING','guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS',
 'mandatory_report':{'checks':17,'pass_run':4,'unknown_not_run':13},'Linux':{'new_source':'PENDING_AFTER_PUSH','parent':json.loads((raw/'ci-parent-final.json').read_text())},
 'Windows':'NOT_RUN','GUI':'NOT_RUN_FULL_PIPELINE','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_UNCONFIRMED_NO_PRESET_PRINTER_OR_CLOUD_EDITS',
 'historical':'Initial zero-length analytic Travel, independent witness reference missing XY growth, wrong focused executable selection and two mistyped helper paths retained. Corrected invocations pass; runtime geometry/margins/limits/negative assertions unchanged.',
 'archive_command':['python3',str((raw/'archive.py').relative_to(root))],'commands':commands,'nonzero_commands':[c for c in commands if c['exit_code']!=0],
 'excluded_commands_from_final_claim':['deposition-cli1.txt.json','focus-final.txt.json','focus5.txt.json','material-cli-final.txt.json','material-cli-final2.txt.json'],
 'raw_sha256':raw_hashes,'raw_to_archive':mapping,'transport':'GZIP_LOG_TXT_GCODE_AND_LARGE_FILES_EXACT_RAW_BYTES_NO_FORCE_ADD_NO_WHITESPACE_TRIMMING'}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p,h in m['archive_sha256'].items():assert sha(root/p)==h
for p,h in raw_hashes.items():
 archive=root/mapping[p];digest=hashlib.sha256()
 with (gzip.open(archive,'rb') if archive.suffix=='.gz' else archive.open('rb')) as f:
  for chunk in iter(lambda:f.read(1048576),b''):digest.update(chunk)
 assert digest.hexdigest()==h,p
print(json.dumps({'status':'PASS_FROZEN_LOSSLESS_EVIDENCE','files':len(m['archive_sha256'])+1,'commands':len(commands),'sources':len(files),'dependencies':len(dependencies),'raw_mappings':len(mapping),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
