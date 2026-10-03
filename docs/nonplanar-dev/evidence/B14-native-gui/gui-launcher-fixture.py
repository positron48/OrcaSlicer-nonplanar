#!/usr/bin/env python3
import os
from pathlib import Path
import sys
bundle = Path(__file__).resolve().parents[2]
runtime = bundle.parent / 'runtime'
if any(a.startswith('--datadir') for a in sys.argv[1:]):
    raise SystemExit('The experimental launcher owns its isolated data directory')
os.chdir(runtime)
environment = dict(os.environ, TMPDIR=str(runtime / 'tmp') + '/', XDG_CACHE_HOME=str(runtime / 'cache'), SLIC3R_NPTOP_LAB='1')
os.execve('/usr/bin/sandbox-exec', ['sandbox-exec', '-f', str(runtime / 'offline.sb'),
          str(bundle / 'Contents/MacOS/NonplanarTopLabCore'), '--datadir', str(runtime / 'data'),
          *sys.argv[1:], '/Users/antonfilatov/www/my/orca-nonplanar/build/nonplanar-evidence/B14-native-gui/gui-fixture1/output/simulation-source.3mf'], environment)
