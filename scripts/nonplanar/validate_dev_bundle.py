#!/usr/bin/env python3
"""Check dev identity and actual OS write/network isolation before GUI launch."""
import argparse
import hashlib
import json
import plistlib
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--bundle', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
bundle = args.bundle.resolve()
base = bundle.parent
manifest = json.loads((base / 'bundle-manifest.json').read_text())
info = plistlib.loads((bundle / 'Contents/Info.plist').read_bytes())
assert info['CFBundleIdentifier'].startswith('local.nonplanartoplab.experimental')
assert 'CFBundleDocumentTypes' not in info and 'CFBundleURLTypes' not in info
core = bundle / 'Contents/MacOS/NonplanarTopLabCore'
assert manifest['binary_sha256'] == hashlib.sha256(core.read_bytes()).hexdigest()
runtime = base / 'runtime'
policy = runtime / 'offline.sb'
outside = base / 'forbidden-sandbox-probe.txt'
assert not outside.exists()
probe = r'''
import errno,json,socket,sys
from pathlib import Path
runtime,outside=map(Path,sys.argv[1:])
inside=runtime/'allowed-sandbox-probe.txt'
inside.write_text('isolated runtime write')
denied_write=False
try:
    with outside.open('x') as f: f.write('unexpected permission')
except OSError as exc:
    denied_write=exc.errno in (errno.EPERM,errno.EACCES)
denied_network=False
try:
    with socket.socket(socket.AF_INET,socket.SOCK_STREAM) as sock:
        sock.settimeout(1)
        sock.connect(('127.0.0.1',9))
except OSError as exc:
    denied_network=exc.errno in (errno.EPERM,errno.EACCES)
print(json.dumps({'inside_write':inside.read_text()=='isolated runtime write',
                  'outside_write_denied':denied_write,'network_denied':denied_network}))
raise SystemExit(0 if denied_write and denied_network else 2)
'''
command = ['/usr/bin/sandbox-exec', '-f', str(policy), sys.executable, '-I', '-B', '-c', probe,
           str(runtime), str(outside)]
result = subprocess.run(command, capture_output=True, text=True, timeout=10)
assert result.returncode == 0, (result.returncode, result.stdout, result.stderr)
assert not outside.exists()
launcher = bundle / 'Contents/MacOS/NonplanarTopLab'
override = subprocess.run([str(launcher), '--datadir', str(outside)], capture_output=True, text=True, timeout=10)
assert override.returncode != 0 and 'owns its isolated data directory' in override.stderr
assert not outside.exists()
report = {'status': 'PASS', 'manifest': manifest, 'sandbox': json.loads(result.stdout),
          'probe_command': command, 'probe_exit_code': result.returncode, 'probe_stderr': result.stderr,
          'datadir_override_exit_code': override.returncode, 'datadir_override_stderr': override.stderr,
          'gui': 'NOT_RUN'}
assert not args.output.exists()
args.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({'status': 'PASS', 'report': str(args.output), 'gui': 'NOT_RUN'}))
