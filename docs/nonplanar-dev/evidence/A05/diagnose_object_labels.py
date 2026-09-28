#!/usr/bin/env python3
"""Probe object labels. This never changes the strict baseline gate."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(root / 'scripts/nonplanar'))
from compare_baselines import normalized, replay

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--label', default='A05-label-diagnostic')
parser.add_argument('--repeat', type=int, default=1)
parser.add_argument('--normal-allocation', action='store_true')
args = parser.parse_args()
assert args.label.replace('-', '').isalnum() and 1 <= args.repeat <= 8
base = root / 'build/nonplanar-evidence' / args.label
base.mkdir(exist_ok=False)
policy = base / 'offline.sb'
policy.write_text('(version 1)\n(allow default)\n(deny network*)\n(deny file-write*)\n'
                  f'(allow file-write* (subpath {json.dumps(str(base))}))\n'
                  '(allow file-write* (literal "/dev/null"))\n')
reference = root / 'tests/nonplanar/data/stock-v2.4.2'
original = root / 'build/nonplanar-evidence/A05-fork/shallow_sphere-off'
command = json.loads((original / 'run.log.json').read_text())['command']
source_gcode = (reference / 'shallow_sphere-off/output/plate_1.gcode').read_text()
source_lines = normalized(source_gcode).splitlines()
label = re.compile(r'^; (?:stop )?printing object shallow_sphere\.stl id:\d+ copy 0$')
result = {'status': 'DIAGNOSTIC_ONLY_NOT_A_GATE', 'runs': []}
binaries = {
    'stock': root / 'build/nonplanar-dev/NonplanarTopLab.app/Contents/MacOS/NonplanarTopLabCore',
    'fork': root / 'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer',
}
for name, binary in [(name if args.repeat == 1 else f'{name}-{i}', binary)
                     for name,binary in binaries.items() for i in range(args.repeat)]:
    work = base / name
    for directory in ('data', 'tmp', 'output'):
        (work / directory).mkdir(parents=True)
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    if name.startswith('stock'):
        assert digest == json.loads((reference / 'manifest.json').read_text())['binary_sha256']
    argv = [arg.replace(str(original), str(work)) for arg in command]
    argv[argv.index('-f')+1] = str(policy)
    argv[argv.index('--datadir')-1] = str(binary)
    if not args.normal_allocation:
        argv.insert(1, 'MallocPreScribble=1')
    subprocess.run([sys.executable, str(root / 'scripts/nonplanar/run_logged.py'),
                    '--output', str(work / 'run.log'), '--', *argv], cwd=work, check=True)
    actual = (work / 'output/plate_1.gcode').read_text()
    lines = normalized(actual).splitlines()
    differences = [(a,b) for a,b in zip(source_lines,lines) if a != b]
    assert len(source_lines) == len(lines)
    assert all(label.fullmatch(a) and label.fullmatch(b) for a,b in differences)
    moves_match = replay(source_gcode) == replay(actual)
    settings_match = (work/'resolved.json').read_bytes() == (reference/'shallow_sphere-off/resolved.json').read_bytes()
    assert moves_match and settings_match
    result['runs'].append({'name': name, 'binary_sha256': digest,
        'strict_timestamp_only_match': not differences, 'changed_comment_lines': len(differences),
        'object_ids': sorted(set(re.findall(r'printing object .* id:(\d+) copy',actual))),
        'modal_moves_match': moves_match, 'resolved_settings_match': settings_match})
(base/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
