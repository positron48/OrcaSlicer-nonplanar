from pathlib import Path
import datetime,gzip,hashlib,json,platform,shutil,subprocess,xml.etree.ElementTree as E
root=Path.cwd();stage='B04-partition-order';raw=root/'build/nonplanar-evidence'/stage;dest=root/'docs/nonplanar-dev/evidence'/stage
assert not dest.exists(),'Never overwrite frozen evidence'
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
files=subprocess.check_output(['git','diff','--cached','--name-only'],text=True).splitlines();assert len(files)==9
parent=json.loads((root/'docs/nonplanar-dev/evidence/B12-supported-deposition/source-manifest.json').read_text())
dependencies=sorted((set(parent['dependency_sha256'])|set(parent['source_sha256']))-set(files));binaries=list(parent['binary_sha256'])
assert len(binaries)==4 and all((root/p).is_file() for p in files+dependencies+binaries)
qualified=['build1','focus1','native-focus1','ctest-qualified','native-polyline-cli-final','native-contour-cli-final','native-contact-cli-final','native-deposition-cli-final','native-travel-cli-final','baseline-capture-final','baseline-compare-final','rate-cli-final','material-cli-final','cover-cli-final','joined-cli-final','nominal-cli-final','support-cli-final','travel-cli-final','deposition-cli-final','contact-cli-final','polyline-cli-final','provenance','job-context-final','job-artifact-final','native-report-oracle','strict-final','compiler','package-final','source-audit-final','authored-diff-check','workflow-qualified','candidate-report-oracle','candidate-report-invalid-material-oracle','candidate-report-failed-material-oracle','native-lineage-report-oracle','native-departure-report-oracle','strict-partition','supported-cli-final','native-supported-cli-final','cli-regressions']
for name in qualified:assert json.loads((raw/(name+'.txt.json')).read_text())['exit_code']==0,name
for p in files:assert hashlib.sha256(subprocess.check_output(['git','show',':'+p])).hexdigest()==sha(root/p),p
suites={}
for p in raw.glob('*.xml'):
 s=E.parse(p).getroot().find('testsuite');suites[p.stem]={'cases':len(s.findall('testcase')),'assertions':int(s.get('tests')),'failures':int(s.get('failures')),'skipped':int(s.get('skipped'))}
assert suites['native-focus1']=={'cases':30,'assertions':2433417,'failures':0,'skipped':0}
ctest=E.parse(raw/'ctest-qualified/results.xml').getroot();assert ctest.get('tests')=='450' and ctest.get('failures')=='0' and ctest.get('skipped')=='0'
assert len(json.loads((raw/'ctest-qualified/discovery.json').read_text())['tests'])==450
for case in ctest.findall('testcase'):assert case.get('status')=='run' and all(case.find(tag) is None for tag in ('skipped','failure','error'))
comparison=json.loads((raw/'baseline-compare-final.txt').read_text());assert len(comparison)==6 and all(x['differential']=='PASS' for x in comparison)
base=root/'build/nonplanar-evidence/B04-partition-order-final-baselines';assert json.loads((base/'manifest.json').read_text())['binary_sha256']==sha(root/'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
provenance=json.loads((raw/'provenance.json').read_text());assert len(provenance['old_files'])==51
assert sum(v['exact'] for v in provenance['old_files'].values())==39 and len(provenance['all_focus_ctest_outputs_exact'])==33
assert suites['focus1']=={'cases':6,'assertions':6319,'failures':0,'skipped':0}
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
strict_partition=[v for v in (raw/'strict-partition.txt').read_text().splitlines() if 'VolumePartitionExact.cpp.o -c ' in v]
assert len(strict_partition)==1 and '-fno-fast-math' in strict_partition[0] and '-ffp-contract=off' in strict_partition[0] and '-include' not in strict_partition[0]
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
m={'schema':1,'stage':stage,'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'git_parent':subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),'git_worktree':'SOURCE_BEFORE_CURRENT_COMMIT','commit_resolution':'git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B04-partition-order.md',
 'source_sha256':{p:sha(root/p) for p in files},'dependency_sha256':{p:sha(root/p) for p in dependencies},'binary_sha256':{p:sha(root/p) for p in binaries},'platform':platform.platform(),'compiler':(raw/'compiler.txt').read_text().strip(),'suites':suites,'final_build':'build1.txt.json','partition_focus':'focus1.xml','final_focus':'native-focus1.xml',
 'ctest_final':{'tests':450,'failures':0,'skipped':0,'elapsed_seconds':json.loads((raw/'ctest-qualified.txt.json').read_text())['elapsed_seconds'],'evidence':'ctest-qualified/results.xml','selection':'PREVIOUS_NATIVE_DISCOVERY_PLUS_ACTUAL_PARTITION_ORDER_REGRESSION'},'OFF_ZAA_final':comparison,
 'runtime_contracts':{'supported_deposition':1,'support_fields':{'root':5,'join':6,'run':3,'support_policy':10},'standalone_support':'UNCHANGED_PRE_RUN_SCOPE','scene_query':1,'diagnostic_contact_versions':[1,2],'contact_fields':{'v1':16,'v2':17},'production_projects_profiles_previous_formats':'UNCHANGED'},
 'invariant':'ACTUAL_DERIVED_INDEXED_BODY_CAP_EXACT_XYZ_ORDER_SHARED_FLOAT_POINTS_ALL_FACES_CYCLIC_WINDING_PRESERVING_REMAP_AND_ORIENTED_FACE_ORDER_ORIGINAL_COORDINATES_TRIANGLES_VOLUME_INTERFACE_ERROR_STOP_GUARDS_NO_HASH_NORMALIZATION_OR_PROOF_REUSE',
 'qualified_scope':'DESCRIPTOR_ORDER_STABILITY_IDENTICAL_ORIENTED_TRIANGULATION_ONLY_UNIVERSAL_CGAL_CROSS_PLATFORM_SOFTWARE_RESOURCE_GUI_WHOLE_JOB_PUBLICATION_AND_PHYSICAL_QUALIFICATION_OPEN','native_outputs':provenance,'native_supported':json.loads((raw/'final-candidate-evidence/native-final-supported-deposition.support-proof.json').read_text()),'CLI_regressions':json.loads((raw/'cli-regressions.json').read_text()),'original_outputs':provenance,'native_CLI':native_cli,'native_contour_CLI':contour,
 'independent_oracle':'AFFINE_VOLUME_BARYCENTRIC_RAYS_HOLE_STEPS_INTERFACE_ORIENTED_COORDINATE_TRIANGLE_MULTISET_PLUS_FRESH113_BIT_FINAL_TEXT_COMPLETE_GEOMETRY_SUPPORT_PARTITIONS_AND_SEPARATE_PROCESS_BYTE_HASH_IDENTITY','CLI_cases':cli,'strict_flags':'VolumePartitionExact.cpp and LinearMaterial.cpp no PCH -fno-fast-math -ffp-contract=off; MSVC NOT_RUN','software_label':'CONFIGURED_PARENT9A3ED1CA_EXACT_CURRENT_SOURCE_DEPENDENCY_BINARY_HASHES_RESOLVER_UNQUALIFIED',
 'review':'SEPARATE_AUTHOR_CRITICAL_AUDIT_INDEPENDENT_PENDING','guarded_export':'BLOCK','full_B01_B15':'IN_PROGRESS','mandatory_report':{'checks':17,'pass_run':4,'unknown_not_run':13},'Linux':{'new_source':'PENDING_AFTER_PUSH','parent':json.loads((raw/'ci-parent.json').read_text())},'Windows':'NOT_RUN','GUI':'NOT_RUN_FULL_PIPELINE','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_KNOWN_GEOMETRY_UNCONFIRMED_NO_PRESET_PRINTER_CLOUD_EDITS',
 'historical':'Test-only ambiguous detail namespace and wrong vector type build errors corrected; expected-red old descriptor ordering retained. New actual derived indices change twelve parent files including material source fingerprints; oriented geometry comparison is evidence only, never normalization or proof reuse. Three nominal UNKNOWN deadline counters retain actual values. Original margins/limits/refusals remain.',
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
