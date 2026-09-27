#!/usr/bin/env python3
"""Capture software-only stock OFF/ZAA outputs in a macOS offline sandbox."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--label', required=True)
    parser.add_argument('--models', nargs='+', default=['flat_block', 'wedge_5deg', 'shallow_sphere'])
    args = parser.parse_args()
    if not args.label.replace('-', '').isalnum():
        parser.error('label must contain only letters, digits and hyphens')
    if any(m not in ('flat_block', 'wedge_5deg', 'shallow_sphere') for m in args.models):
        parser.error('only the three repository-owned baseline models are supported')
    root = Path(__file__).resolve().parents[2]
    app = root / 'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer'
    base = root / 'build/nonplanar-evidence' / args.label
    base.mkdir(parents=True, exist_ok=False)
    policy = base / 'offline.sb'
    policy.write_text('(version 1)\n(allow default)\n(deny network*)\n(deny file-write*)\n'
                      f'(allow file-write* (subpath {json.dumps(str(base))}))\n'
                      '(allow file-write* (literal "/dev/null"))\n')
    inputs = root / 'tests/nonplanar/data/baseline'
    manifest = {'binary_sha256': sha256(app), 'verification': 'NOT_RUN', 'runs': []}
    for model in args.models:
        source = root / 'docs/nonplanar/fixtures/models' / (model + '.stl')
        for mode in ('off', 'zaa'):
            work = base / f'{model}-{mode}'
            work.mkdir()
            for name in ('data', 'tmp', 'output'):
                (work / name).mkdir()
            process = inputs / f'process-{mode}.json'
            cmd = ['env', 'TMPDIR=' + str(work / 'tmp') + '/', 'sandbox-exec', '-f', str(policy), str(app),
                   '--datadir', str(work / 'data'), '--debug', '2',
                   '--load-settings', str(inputs / 'machine.json') + ';' + str(process),
                   '--load-filaments', str(inputs / 'filament.json'), '--arrange', '1', '--orient', '0',
                   '--outputdir', str(work / 'output'), '--export-settings', str(work / 'resolved.json'),
                   '--slice', '0', str(source)]
            result = subprocess.run([sys.executable, str(root / 'scripts/nonplanar/run_logged.py'),
                                     '--output', str(work / 'run.log'), '--', *cmd], cwd=work)
            record = {'model': model, 'mode': mode, 'exit_code': result.returncode,
                      'input_sha256': {str(p.relative_to(root)): sha256(p) for p in
                                       (source, inputs / 'machine.json', process, inputs / 'filament.json')},
                      'files': {str(p.relative_to(base)): sha256(p) for p in work.rglob('*') if p.is_file()}}
            manifest['runs'].append(record)
            (base / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
            if result.returncode:
                return result.returncode
    return 0


if __name__ == '__main__':
    sys.exit(main())
