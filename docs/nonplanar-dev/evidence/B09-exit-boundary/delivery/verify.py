from pathlib import Path
import gzip,hashlib,json,subprocess
root=Path.cwd();base=root/'docs/nonplanar-dev/evidence/B09-exit-boundary'
m=json.loads((base/'source-manifest.json').read_text());d=json.loads((base/'delivery/delivery-manifest.json').read_text())
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
assert sha(base/'source-manifest.json')==d['original_manifest_sha256']
assert len(d['archive_fallback'])==12
tracked=set(subprocess.check_output(['git','ls-files'],text=True).splitlines())
for field in ('source_sha256','dependency_sha256','binary_sha256'):
 for p,h in m[field].items():assert sha(root/p)==h,(field,p)
for p,h in m['source_sha256'].items():
 assert hashlib.sha256(subprocess.check_output(['git','show','HEAD:'+p])).hexdigest()==h,p
for p,h in m['archive_sha256'].items():
 path=d['archive_fallback'].get(p,p);assert path in tracked,path
 actual=gzip.decompress((root/path).read_bytes()) if p in d['archive_fallback'] else (root/path).read_bytes()
 assert hashlib.sha256(actual).hexdigest()==h,p
for p,h in d['compressed_sha256'].items():assert p in tracked and sha(root/p)==h,p
for p,h in m['raw_sha256'].items():
 original=m['raw_to_archive'][p];path=d['archive_fallback'].get(original,original);assert path in tracked,path
 with (gzip.open(root/path,'rb') if path.endswith('.gz') else (root/path).open('rb')) as f:assert hashlib.sha256(f.read()).hexdigest()==h,p
print(json.dumps({'status':'PASS_COMMITTED_MEMBERSHIP_AND_EXACT_SOURCE_BINARY_ARCHIVE_LOSSLESS_RAW_TRANSPORT','sources':len(m['source_sha256']),'dependencies':len(m['dependency_sha256']),'binaries':len(m['binary_sha256']),'original_archive_members':len(m['archive_sha256']),'explicit_fallbacks':len(d['archive_fallback']),'lossless_raw_mappings':len(m['raw_sha256'])}))
