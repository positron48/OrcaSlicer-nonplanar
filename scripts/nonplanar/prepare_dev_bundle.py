#!/usr/bin/env python3
"""Make a local CLI-only dev bundle; never installs or registers file/URL handlers."""
from pathlib import Path
import json
import plistlib
import shutil

root = Path(__file__).resolve().parents[2]
source = root / 'build/arm64/src/Release/OrcaSlicer.app'
destination = root / 'build/nonplanar-dev/NonplanarTopLab.app'
if destination.exists():
    raise SystemExit('Refusing to overwrite an existing dev bundle')
shutil.copytree(source, destination, symlinks=True)
plist_path = destination / 'Contents/Info.plist'
with plist_path.open('rb') as stream:
    info = plistlib.load(stream)
info.update(CFBundleIdentifier='local.nonplanartoplab.experimental',
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
print(destination)
