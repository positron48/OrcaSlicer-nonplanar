from pathlib import Path
import gzip,hashlib,json,shutil,subprocess
root=Path.cwd();base=root/'docs/nonplanar-dev/evidence/B09-exit-boundary';m=json.loads((base/'source-manifest.json').read_text())
tracked=set(subprocess.check_output(['git','ls-files',str(base.relative_to(root))],text=True).splitlines())
missing=sorted(set(m['archive_sha256'])-tracked);assert len(missing)==12
mapping={};compressed={}
for name in missing:
 source=root/name;assert source.suffix=='.log' and hashlib.sha256(source.read_bytes()).hexdigest()==m['archive_sha256'][name]
 target=Path(str(source)+'.gz');assert not target.exists()
 with source.open('rb') as src,target.open('xb') as dst,gzip.GzipFile(filename='',mode='wb',fileobj=dst,mtime=0) as gz:shutil.copyfileobj(src,gz)
 assert hashlib.sha256(gzip.decompress(target.read_bytes())).hexdigest()==m['archive_sha256'][name]
 rel=str(target.relative_to(root));mapping[name]=rel;compressed[rel]=hashlib.sha256(target.read_bytes()).hexdigest()
supplement={'schema':1,'scope':'EXACT_LOSSLESS_TRANSPORT_REPAIR_ONLY_SOURCE_GEOMETRY_BUDGETS_AND_TESTS_UNCHANGED','original_manifest':'source-manifest.json','original_manifest_sha256':hashlib.sha256((base/'source-manifest.json').read_bytes()).hexdigest(),'omitted_reason':'REPOSITORY_GITIGNORE_STAR_LOG','archive_fallback':mapping,'compressed_sha256':compressed,'authored_source_count':13,'raw_artifact_whitespace':'EXPECTED_EXIT2_LOSSLESS_CATCH_BLANK_LINES_SEPARATE_AUTHORED_CHECK_PASS','source_build_tests':'ORIGINAL_MANIFEST_EXACT_UNCHANGED','no_force_add':True}
(base/'delivery/delivery-manifest.json').write_text(json.dumps(supplement,indent=2)+'\n')
print(json.dumps({'status':'PASS_LOSSLESS_TRANSPORT_REPAIR','members':len(mapping)}))
