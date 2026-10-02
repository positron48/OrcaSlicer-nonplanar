from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();stage='B13-native-departure';raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==15
parent=json.loads((root/'docs/nonplanar-dev/evidence/B09-native-departure/source-manifest.json').read_text())
dependencies=sorted((set(parent['dependency_sha256'])|set(parent['source_sha256']))-set(files));binaries=list(parent['binary_sha256'])
assert len(binaries)==4 and all((root/p).is_file() for p in files+dependencies+binaries)
qualified=['build-final','focus-final','ctest-final2','baseline-capture-final','baseline-compare-final','oracle-final','oracle-original-native','oracle-mutations-final','provenance-final2','job-context-final','job-artifact-final','strict-job','compiler','package-final','source-audit-final','authored-diff-check']
for name in qualified:assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
for p in files:assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==sha(root/p),p
suites={}
for p in raw.glob('*.xml'):
 s=E.parse(p).getroot().find('testsuite');suites[p.stem]={'cases':len(s.findall('testcase')),'assertions':int(s.get('tests')),'failures':int(s.get('failures')),'skipped':int(s.get('skipped'))}
assert suites['focus-final']=={'cases':17,'assertions':947083,'failures':0,'skipped':0}
ctest=E.parse(raw/'ctest-final2/results.xml').getroot();assert ctest.get('tests')=='425' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest-final2/discovery.json').read_text())['tests'])==425
for case in ctest.findall('testcase'):
 assert case.get('status')=='run' and all(case.find(tag) is None for tag in ('skipped','failure','error'))
native=next(t for t in ctest.findall('testcase') if 'native first footprint' in t.get('name',''))
new=next(t for t in ctest.findall('testcase') if 'native departure binds' in t.get('name',''))
assert '1148462 assertions' in native.find('system-out').text and '946528 assertions' in new.find('system-out').text
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=root/'build/nonplanar-evidence/B13-native-departure-final-baselines';assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
provenance=json.loads((raw/'provenance-final2.json').read_text());assert provenance['differences']==[] and len(provenance['original_files'])==19
assert provenance['new_candidate_dependency_identity']['all_actual_rows_and_candidate_bytes']=='EXACT'
mutations=json.loads((raw/'oracle-mutations-final.json').read_text());assert mutations['positives']==6 and mutations['refusals']==44
record=json.loads((raw/'final-job-evidence/native-departure-report.json').read_text());assert record['candidate_sha256']=='b8eb15681127c1a8db57ce0aa257326763f16832b82fdafd13ed091c4bd98ef8'
assert record['records']==2205 and len(record['candidate_bytes'].encode())==128014
strict=[s for s in (raw/'strict-job.txt').read_text().splitlines() if 'Nonplanar/Job' in s and '.cpp.o -c ' in s]
assert len(strict)==2
for s in strict:assert '-fno-fast-math' in s and '-ffp-contract=off' in s and '-include' not in s
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
 'commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B13-native-departure.md',
 'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},
 'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'suites':suites,
 'final_build':'build-final.txt.json','final_focus':'focus-final.xml',
 'ctest_final':{'tests':425,'failures':0,'skipped':0,'elapsed_seconds':json.loads((raw/'ctest-final2.txt.json').read_text())['elapsed_seconds'],'evidence':'ctest-final2/results.xml','selection':'EXACT_ORIGINAL_NONPLANAR_FFF_DISCOVERY_INDICES_PLUS_THREE_DEPARTURE_LINEAGE_CASES'},
 'OFF_ZAA_final':comparison,'runtime_contracts':{'departure_identity':1,'native_plan':2,'candidate_manifest':3,'old_native_opaque_and_production_formats':'UNCHANGED'},
 'invariant':'CURRENT_NATIVE_JOB_ATTEMPT_COMPLETE_BODY_HATCH_CAP_EXACT_LAID_DEPARTURE_OWNER_EXACT_MOTION_INPUT_ALL_ROUTED_ROWS_CONTEXT_ONLY_LIMIT_REDUCTIONS_FINAL_BYTES_CANONICAL_EDGES',
 'qualified_scope':'BOUNDED_NATIVE_DEPENDENCY_LINEAGE_AND_DECLARED_RATE_MATERIAL_REPLAY_ONLY_NO_COMPLETE_CAP_CONTACT_WHOLE_ROUTE_BYTE_GEOMETRY_JOB_EXPORT_OR_PHYSICAL_QUALIFICATION',
 'native':{'original_files':provenance['original_files'],'new_departure':provenance['native_departure'],'new_dependency_identity':provenance['new_candidate_dependency_identity'],
 'original_case_assertions':1148462,'original_case_seconds':float(native.get('time')),'new_positive_assertions':946528,'new_positive_seconds':float(new.get('time'))},
 'independent_oracles':{'CPP':'113_BIT_WHOLE_LEAF_HEAD_BEAD_EQUATIONS_ALL_PAIR_COMPLETE_DISJOINT_PARTITIONS','Python':mutations,'identity':provenance,
 'final_bytes':'EXISTING_INDEPENDENT_RATE_POSE_E_MATERIAL_COMPONENT_REPLAY_HEAD_CONTACT_GEOMETRY_NOT_RUN','job_context':'PASS','candidate_artifact':'PASS'},
 'strict_flags':'JobNative.cpp and JobArtifact.cpp no PCH; -fno-fast-math -ffp-contract=off; MSVC registration unchanged NOT_RUN',
 'software_label':'CONFIGURED_PARENT9A3ED1CA_EXACT_CURRENT_SOURCE_DEPENDENCY_BINARY_HASHES_NO_QUALIFIED_RESOLVER',
 'review':'SEPARATE_AUTHOR_CRITICAL_AUDIT_INDEPENDENT_PENDING','guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS',
 'mandatory_report':{'checks':17,'pass_run':4,'unknown_not_run':13},'CLI_100':'NOT_RUN_THIS_STAGE_UNCHANGED_INDEPENDENT_VERIFIER_SOURCE_FORMATS_PARENT_RESULTS_SEPARATE',
 'Linux':{'new_source':'PENDING_AFTER_PUSH','inherited':json.loads((raw/'ci-inherited.json').read_text()),'failure':json.loads((raw/'ci-parent-failure.json').read_text()),
 'fix':'OLD_LITERAL_MACOS_REPORT_COUNT2218_FAILED_ACTUAL_LINUX2257_NOW_EXACT_ACTUAL_JOURNAL_SIZE_PER_EVENT_CHECKS_UNCHANGED'},
 'Windows':'NOT_RUN','GUI':'NOT_RUN_FULL_PIPELINE','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_UNCONFIRMED_NO_PRESET_PRINTER_OR_CLOUD_EDITS',
 'historical':'Initial final provenance helper incorrectly required equal full new reports across runs. Every actual row/candidate byte and job/request/geometry/scene/policy/map matches, derived body/dependent identity differs and remains unqualified. Both exact graphs validate separately, no runtime normalization/reuse. Failed helper retained and excluded from final claims. Inherited Linux35503607 count assertion failure retained, corrected actual-journal assertion not yet Linux-verified.',
 'archive_command':['python3',str((raw/'archive.py').relative_to(root))],'commands':commands,'nonzero_commands':[c for c in commands if c['exit_code']!=0],
 'excluded_commands_from_final_claim':['provenance-final.txt.json'],
 'raw_sha256':raw_hashes,'raw_to_archive':mapping,'transport':'GZIP_LOG_TXT_GCODE_AND_LARGE_FILES_EXACT_RAW_BYTES_NO_FORCE_ADD_NO_WHITESPACE_TRIMMING'}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p,h in m['archive_sha256'].items():assert sha(root/p)==h
for p,h in raw_hashes.items():
 archive=root/mapping[p];digest=hashlib.sha256()
 with (gzip.open(archive,'rb') if archive.suffix=='.gz' else archive.open('rb')) as f:
  for chunk in iter(lambda:f.read(1048576),b''):digest.update(chunk)
 assert digest.hexdigest()==h,p
print(json.dumps({'status':'PASS_FROZEN_LOSSLESS_EVIDENCE','files':len(m['archive_sha256'])+1,'commands':len(commands),'sources':len(files),'dependencies':len(dependencies),'raw_mappings':len(mapping),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
