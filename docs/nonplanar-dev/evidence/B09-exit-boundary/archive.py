from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();stage='B09-exit-boundary';raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==13
parent=json.loads((root/'docs/nonplanar-dev/evidence/B13-native-lineage/source-manifest.json').read_text())
dependencies=sorted((set(parent['dependency_sha256'])|set(parent['source_sha256'])|{'docs/nonplanar/SAFETY_AND_VERIFICATION.md','src/libslic3r/Nonplanar/DepositionModel.hpp','tests/nonplanar/test_profile_scene.cpp'})-set(files));binaries=list(parent['binary_sha256']);assert len(binaries)==4
assert all((root/p).is_file() for p in files+dependencies+binaries)
suites={}
for p in raw.glob('*.xml'):
 s=E.parse(p).getroot().find('testsuite');suites[p.stem]={'cases':len(s.findall('testcase')),'assertions':int(s.get('tests')),'failures':int(s.get('failures')),'skipped':int(s.get('skipped'))}
assert suites['focus-final']=={'cases':7,'assertions':1002,'failures':0,'skipped':0}
ctest=E.parse(raw/'ctest-final/results.xml').getroot();assert ctest.get('tests')=='420' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest-final/discovery.json').read_text())['tests'])==420
for name in ['build-final','focus-final','ctest-final','independent-witness','provenance-compare','native-report-oracle','native-lineage-oracle','baseline-capture-final','baseline-compare-final','package','source-audit','strict-material','compiler','authored-diff-check']:
 assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=root/'build/nonplanar-evidence/B09-exit-boundary-final-baselines';assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
provenance=json.loads((raw/'provenance-comparison.json').read_text());assert provenance['differences']==[] and len(provenance['original_files'])==14
point=json.loads((raw/'independent-witness-result.json').read_text());assert point['positive']['material_record']==2210 and point['positive']['parameter']=='0' and len(point['refusals'])==6
witness=json.loads((raw/'final-ctest-native/native-exit-witness.json').read_text());assert witness['status']=='FAIL' and witness['parameter']==0 and witness['component_index']==0 and witness['event_index']==2218
command=next(s for s in (raw/'strict-material.txt').read_text().splitlines() if 'DepositionModel.cpp.o -c ' in s)
assert '-fno-fast-math' in command and '-ffp-contract=off' in command and '-include' not in command and '9a3ed1ca' in command
assert 'NPTOP_TEMP_BOUNDARY_DIAGNOSTIC' not in (root/'src/libslic3r/Nonplanar/DepositionModel.cpp').read_text()
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
m={'schema':1,'stage':stage,'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT','commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B09-annulus-endpoints.md',
 'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'suites':suites,
 'final_build':'build-final.txt.json','final_focus':'focus-final.xml','ctest_final':{'tests':420,'failures':0,'skipped':0,'elapsed_seconds':96.085,'evidence':'ctest-final/results.xml','selection':'EXACT_ORIGINAL_NONPLANAR_FFF_DISCOVERY_INDICES_PLUS_ONE_ENDPOINT_CASE'},'OFF_ZAA_final':comparison,
 'runtime_contracts':{'material_motion':2,'simulation_lift_route':1,'production_formats':'UNCHANGED'},'invariant':'POINTS_ONLY_REFUTE_STRICT_ORIGINAL_TOOL_AND_UPPER_MEMBERSHIP_FULL_CONTINUOUS_PASS_PARTITION_MARGINS_CURRENT_PROGRESS_FUTURE_EXCLUSION_AND_ORIGINAL_SHARED_BUDGETS_UNCHANGED',
 'qualified_scope':'DECLARED_MATERIAL_ENVELOPE_POINT_REFUTATION_NOT_MEASURED_PHYSICAL_NOMINAL_COLLISION_CONTACT_FULL_ROUTE_OR_EXPORT_QUALIFICATION','native':{'exit':witness,'candidate_provenance':provenance,'ctest_native_seconds':9.40835,'ctest_assertions':137829},
 'independent_oracles':{'point':point,'CPP':'113_BIT_OLD_OR_CURRENT_UPPER_ANNULUS_POINT_AND_COMPLETE_HIGH_ROUTE_HEIGHT_INEQUALITIES','native_report_identity':'PASS','native_lineage_identity':'PASS'},'CLI_100':'NOT_RUN_THIS_STAGE_UNCHANGED_INDEPENDENT_VERIFIER_SOURCE_FORMATS_PARENT_RESULTS_SEPARATE',
 'strict_flags':'DepositionModel.cpp no PCH; -fno-fast-math -ffp-contract=off; unchanged MSVC registration NOT_RUN','software_label':'CONFIGURED_PARENT9A3ED1CA_EXACT_CURRENT_SOURCE_DEPENDENCY_BINARY_HASHES_NO_QUALIFIED_RESOLVER','review':'SEPARATE_AUTHOR_CRITICAL_AUDIT_INDEPENDENT_PENDING','guarded_export':'BLOCK','full_B08_B09':'IN_PROGRESS_CONTACT_END_ACCESS_HEIGHT_SEARCH_COMPLETE_JOB_ROUTE_REPLAY_PENDING','full_B01_B15':'IN_PROGRESS',
 'original_budgets_losses_margins_negatives_goldens_unchanged':True,'Linux':{'new_source':'PENDING_AFTER_PUSH','inherited':json.loads((raw/'ci-inherited.json').read_text())},'Windows':'NOT_RUN','GUI':'NOT_RUN_FULL_PIPELINE','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_UNCONFIRMED_NO_PRESET_PRINTER_OR_CLOUD_EDITS',
 'historical':'First diagnostic filter addressed wrong nonplanar consumer and ran zero tests exit2; correct fff consumer retained old UNKNOWN with exact boundary. Independent point then established original Upper collision. Original new regression failed UNKNOWN rather than FAIL. Enhanced native test failed old UNKNOWN expectation; corrected to strict FAIL with independent witness. Author audit replaced literal2210 with authoritative first_later before final build. Temporary instrumentation removed. All failures retained, not interpreted as passing evidence.',
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
