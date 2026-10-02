from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();stage='B09-native-departure';raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==12
parent=json.loads((root/'docs/nonplanar-dev/evidence/B09-exit-boundary/source-manifest.json').read_text())
dependencies=sorted((set(parent['dependency_sha256'])|set(parent['source_sha256']))-set(files));binaries=list(parent['binary_sha256']);assert len(binaries)==4
assert all((root/p).is_file() for p in files+dependencies+binaries)
qualified=['build-final','focus-final','ctest-final2','baseline-capture-final','baseline-compare-final','native-report-oracle','native-lineage-oracle','provenance-compare2','strict-profile','compiler','package','source-audit','authored-diff-check']
for name in qualified:assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
for p in files:assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==sha(root/p),p
suites={}
for p in raw.glob('*.xml'):
 s=E.parse(p).getroot().find('testsuite');suites[p.stem]={'cases':len(s.findall('testcase')),'assertions':int(s.get('tests')),'failures':int(s.get('failures')),'skipped':int(s.get('skipped'))}
assert suites['focus-final']=={'cases':9,'assertions':7354,'failures':0,'skipped':0}
ctest=E.parse(raw/'ctest-final2/results.xml').getroot();assert ctest.get('tests')=='422' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest-final2/discovery.json').read_text())['tests'])==422
native=next(t for t in ctest.findall('testcase') if 'native first footprint' in t.get('name',''))
assert '1148462 assertions' in native.find('system-out').text and native.get('status')=='run'
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=root/'build/nonplanar-evidence/B09-native-departure-final-baselines';assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
provenance=json.loads((raw/'provenance-comparison.json').read_text());assert provenance['differences']==[] and len(provenance['original_files'])==15
departure=json.loads((raw/'final-native2/native-departure.json').read_text());assert departure['candidate_rows']==2221 and departure['work']==186808
assert departure['candidate_sha256']=='163f355dce48633d4fb6ce8ca7df9b265ed6212b6d3fa50d438c9fdd6ae6166f'
assert departure['full_cap_fill']==departure['deposition_contact']==departure['final_byte_geometry']=='NOT_RUN'
command=next(s for s in (raw/'strict-profile.txt').read_text().splitlines() if 'ProfileScene.cpp.o -c ' in s)
assert '-fno-fast-math' in command and '-ffp-contract=off' in command and '-include' not in command and '9a3ed1ca' in command
assert 'NPTOP_TEMP_BOUNDARY_DIAGNOSTIC' not in (root/'src/libslic3r/Nonplanar/DepositionModel.cpp').read_text()
commands=[];raw_hashes={};mapping={};dest.mkdir(parents=True)
for source,prefix in [(raw,Path()),(base,Path('baselines-final'))]:
 for path in sorted(source.rglob('*')):
  if not path.is_file() or path.name.startswith('archive-execution.'):continue
  rel=prefix/path.relative_to(source);name=str(path.relative_to(root));raw_hashes[name]=sha(path)
  if path.suffix=='.json':
   try:record=json.loads(path.read_text())
   except json.JSONDecodeError:record=None
   if isinstance(record,dict) and record.get('status')=='COMPLETED' and 'command' in record and 'exit_code' in record:commands.append(dict(evidence=str(rel),**record))
  # Lossless transport of ignored .log files and raw reporter whitespace.
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
 'commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B09-native-departure.md',
 'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},
 'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'suites':suites,
 'final_build':'build-final.txt.json','final_focus':'focus-final.xml',
 'ctest_final':{'tests':422,'failures':0,'skipped':0,'elapsed_seconds':99.857,'evidence':'ctest-final2/results.xml','selection':'EXACT_ORIGINAL_NONPLANAR_FFF_DISCOVERY_INDICES_PLUS_TWO_DEPARTURE_CASES'},
 'OFF_ZAA_final':comparison,'runtime_contracts':{'simulation_cap_departure':1,'others_and_production_formats':'UNCHANGED'},
 'invariant':'EXACT_COMPLETE_PARENT_AND_SELECTED_PROSPECTIVE_BEAD_NORMAL_SOURCE_GRAPH_IMMUTABLE_ORIGINAL_ROWS_TARGETS_RECALCULATED_MATERIAL_COMPLETE_THREE_LEG_HEAD_ACTUAL_PREFIX_WITH_SHARED_ROOT_LIMITS_AND_LATCHED_REFUSALS',
 'qualified_scope':'SIMULATION_SELECTED_BEAD_ASSEMBLY_AND_DEPARTURE_ONLY_NO_WIDTH_SELECTION_DEPOSITION_CONTACT_FULL_CAP_FILL_BYTE_GEOMETRY_JOB_EXPORT_OR_PHYSICAL_QUALIFICATION',
 'native':{'departure':provenance['departure'],'original_files':provenance['original_files'],'ctest_seconds':float(native.get('time')),'ctest_assertions':1148462,
 'target_volume_mm3':departure['selected_target_volume'],'original_wide_target_volume_mm3':departure['original_wide_target_volume'],'original_whole_target':'UNCHANGED_FULL_FILL_NOT_RUN','width_0_2_replay':'UNKNOWN_UNSUPPORTED_FINAL_ROUNDED_SECTION_ORIGINAL_0_02_DOSE_ERROR_RETAINED'},
 'independent_oracles':{'CPP':'113_BIT_COMPLETE_LEAF_EQUATIONS_AND_ALL_PAIR_DISJOINT_PARTITION_RESTRICTED_FIXTURE_DOMAIN_REFERENCE_EXCLUSION_GUARD1E_20_MEASURE_ACCUMULATION1E_28','final_bytes':'INDEPENDENT_POSE_E_FULL_STOP_RATES_AND_MATERIAL_REPLAY_COMPONENTS_ONLY','identity':provenance,'native_report_identity':'PASS','native_lineage_identity':'PASS'},
 'CLI_100':'NOT_RUN_THIS_STAGE_UNCHANGED_INDEPENDENT_VERIFIER_SOURCE_FORMATS_PARENT_RESULTS_SEPARATE',
 'strict_flags':'ProfileScene.cpp no PCH; -fno-fast-math -ffp-contract=off; unchanged MSVC registration NOT_RUN',
 'software_label':'CONFIGURED_PARENT9A3ED1CA_EXACT_CURRENT_SOURCE_DEPENDENCY_BINARY_HASHES_NO_QUALIFIED_RESOLVER','review':'SEPARATE_AUTHOR_CRITICAL_AUDIT_INDEPENDENT_PENDING',
 'guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS','native_job_binding':'EXTRA_ROUTE_ROWS_NOT_IMPLEMENTED_PROTECTED_EDGE_NEXT_NO_CANONICAL_IDENTITY_RELAXATION',
 'mandatory_report':{'checks':17,'pass_run':4,'unknown_not_run':13},'Linux':{'new_source':'PENDING_AFTER_PUSH','inherited':json.loads((raw/'ci-inherited.json').read_text())},
 'Windows':'NOT_RUN','GUI':'NOT_RUN_FULL_PIPELINE','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_UNCONFIRMED_NO_PRESET_PRINTER_OR_CLOUD_EDITS',
 'historical':'Wrong mutated low-head scene caused first .28/.2 trial failures, corrected immutable original scene succeeds. Compile failures retained. native-replay.txt ran the pre-replay binary after failed replay-build and supplies no final replay claim. Correct rebuilt .2 fails final rounded section at unchanged dose band; retained negative. .28 passes. First full CTest with relative diagnostic directories fails two file writes; absolute-path final2 passes. First identity helper used wrong record hash prefix, retained source/error; corrected canonical-prefix helper passes. Temporary planner instrumentation removed.',
 'archive_command':['python3',str((raw/'archive.py').relative_to(root))],'commands':commands,'nonzero_commands':[c for c in commands if c['exit_code']!=0],
 'excluded_commands_from_final_claim':['native-replay.txt.json','factory-focus.txt.json','ctest-final.txt.json','provenance-compare.txt.json'],
 'raw_sha256':raw_hashes,'raw_to_archive':mapping,'transport':'GZIP_LOG_TXT_GCODE_AND_LARGE_FILES_EXACT_RAW_BYTES_NO_FORCE_ADD_NO_WHITESPACE_TRIMMING'}
m['archive_sha256']={str(p.relative_to(root)):sha(p) for p in sorted(dest.rglob('*')) if p.is_file()};(dest/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p,h in m['archive_sha256'].items():assert sha(root/p)==h
for p,h in raw_hashes.items():
 archive=root/mapping[p];digest=hashlib.sha256()
 with (gzip.open(archive,'rb') if archive.suffix=='.gz' else archive.open('rb')) as f:
  for b in iter(lambda:f.read(1048576),b''):digest.update(b)
 assert digest.hexdigest()==h,p
print(json.dumps({'status':'PASS_FROZEN_LOSSLESS_EVIDENCE','files':len(m['archive_sha256'])+1,'commands':len(commands),'sources':len(files),'dependencies':len(dependencies),'raw_mappings':len(mapping),'bytes':sum(p.stat().st_size for p in dest.rglob('*') if p.is_file())}))
