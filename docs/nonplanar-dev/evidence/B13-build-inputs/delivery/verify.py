from pathlib import Path
import argparse,gzip,hashlib,json,subprocess
parser=argparse.ArgumentParser();parser.add_argument('--revision',default='HEAD');args=parser.parse_args()
root=Path.cwd();base=root/'docs/nonplanar-dev/evidence/B13-build-inputs'
m=json.loads((base/'source-manifest.json').read_text());d=json.loads((base/'delivery/manifest.json').read_text())
def sha(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
tracked=set(subprocess.check_output(['git','ls-files'],text=True).splitlines())
assert sha(base/'source-manifest.json')==d['source_manifest_sha256']
assert str((base/'source-manifest.json').relative_to(root)) in tracked
for field in ('source_sha256','dependency_sha256','binary_sha256'):
 for p,h in m[field].items():assert sha(root/p)==h,(field,p)
for p,h in m['source_sha256'].items():
 revision=':'+p if args.revision=='index' else args.revision+':'+p
 assert hashlib.sha256(subprocess.check_output(['git','show',revision])).hexdigest()==h,p
for p,h in {**m['archive_sha256'],**d['extra_sha256']}.items():
 assert p in tracked and sha(root/p)==h,p
for p,h in m['raw_sha256'].items():
 archive=m['raw_to_archive'][p];assert archive in tracked,archive
 digest=hashlib.sha256()
 with (gzip.open(root/archive,'rb') if archive.endswith('.gz') else (root/archive).open('rb')) as f:
  for b in iter(lambda:f.read(1048576),b''):digest.update(b)
 assert digest.hexdigest()==h,p
assert str((base/'delivery/manifest.json').relative_to(root)) in tracked
print(json.dumps({'status':'PASS_GIT_MEMBERSHIP_EXACT_SOURCE_DEPENDENCY_BINARY_ARCHIVE_AND_LOSSLESS_RAW','revision':args.revision,
 'sources':len(m['source_sha256']),'dependencies':len(m['dependency_sha256']),'binaries':len(m['binary_sha256']),
 'archive_members':len(m['archive_sha256'])+1,'delivery_members':len(d['extra_sha256'])+1,'lossless_raw_mappings':len(m['raw_sha256'])}))
