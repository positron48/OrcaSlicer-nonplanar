#!/usr/bin/env python3
"""Make an isolated local dev bundle; never installs or registers file/URL handlers."""
from pathlib import Path
import argparse
import hashlib
import json
import plistlib
import re
import shutil

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--label', help='Fresh, separate bundle/runtime label under build/nonplanar-dev')
args = parser.parse_args()
if args.label is not None and not re.fullmatch(r'[a-z][a-z0-9-]{0,63}', args.label):
    parser.error('label must begin with a letter and contain only lowercase letters, digits or hyphens')
root = Path(__file__).resolve().parents[2]
source = root / 'build/arm64/src/Release/OrcaSlicer.app'
base = root / 'build/nonplanar-dev'
if args.label:
    base = base / args.label
destination = base / 'NonplanarTopLab.app'
if destination.exists():
    raise SystemExit('Refusing to overwrite an existing dev bundle')
shutil.copytree(source, destination, symlinks=True)
plist_path = destination / 'Contents/Info.plist'
with plist_path.open('rb') as stream:
    info = plistlib.load(stream)
info.update(CFBundleIdentifier='local.nonplanartoplab.experimental' + ('.' + args.label if args.label else ''),
            CFBundleExecutable='NonplanarTopLab', CFBundleName='Nonplanar Top Lab Experimental',
            CFBundleDisplayName='Nonplanar Top Lab Experimental')
for key in ('CFBundleDocumentTypes', 'CFBundleURLTypes'):
    info.pop(key, None)
with plist_path.open('wb') as stream:
    plistlib.dump(info, stream)
(destination / 'Contents/MacOS/OrcaSlicer').rename(destination / 'Contents/MacOS/NonplanarTopLabCore')
runtime = destination.parent / 'runtime'
for name in ('data', 'tmp', 'cache'):
    (runtime / name).mkdir(parents=True)
policy = runtime / 'offline.sb'
policy.write_text('(version 1)\n(allow default)\n(deny network*)\n(deny file-write*)\n'
                  f'(allow file-write* (subpath {json.dumps(str(runtime))}))\n'
                  '(allow file-write* (literal "/dev/null"))\n')
launcher = destination / 'Contents/MacOS/NonplanarTopLab'
launcher.write_text('''#!/usr/bin/env python3
import os
from pathlib import Path
import sys
bundle = Path(__file__).resolve().parents[2]
runtime = bundle.parent / 'runtime'
if any(a.startswith('--datadir') for a in sys.argv[1:]):
    raise SystemExit('The experimental launcher owns its isolated data directory')
os.chdir(runtime)
environment = dict(os.environ, TMPDIR=str(runtime / 'tmp') + '/', XDG_CACHE_HOME=str(runtime / 'cache'))
os.execve('/usr/bin/sandbox-exec', ['sandbox-exec', '-f', str(runtime / 'offline.sb'),
          str(bundle / 'Contents/MacOS/NonplanarTopLabCore'), '--datadir', str(runtime / 'data'),
          *sys.argv[1:]], environment)
''')
launcher.chmod(0o755)
(base / 'bundle-manifest.json').write_text(json.dumps({
    'source': str(source), 'destination': str(destination),
    'bundle_identifier': info['CFBundleIdentifier'],
    'binary_sha256': hashlib.sha256((destination / 'Contents/MacOS/NonplanarTopLabCore').read_bytes()).hexdigest(),
    'runtime': str(runtime), 'policy': str(policy),
    'network': 'DENY', 'write_scope': 'RUNTIME_AND_DEV_NULL_ONLY',
    'document_handlers': False, 'url_handlers': False,
    'gui_validation': 'NOT_RUN',
}, indent=2) + '\n')
print(destination)
