#!/usr/bin/env python3
"""Independent predefined canonical config vectors; never a compatibility oracle."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def fixtures():
    bits = lambda value: struct.pack('>d', value).hex()
    string = lambda value: value.encode('utf-8').hex()
    mixed = {
        'bool': [8, False, True],
        'float': [1, False, bits(0.1)],
        'float%': [5, False, [bits(0.1), True]],
        'ints': [16386, False, [-1, 0, 2147483647]],
        'nil': [16385, True, ['7ff8000000000001', '8000000000000000', '0000000000000001']],
        'point': [7, False, [bits(1), bits(2), bits(3)]],
        'text': [16387, False, [string('Привет'), b'line\n\0tail'.hex()]],
        'enum': [9, False, ['generic', [[string('a'), 1], [string('b'), 2]], 2]],
    }
    cases = []
    for name, options in [('empty', {}), ('mixed', mixed)]:
        obj = {'schema': 1, 'options': [[string(k)] + v for k, v in sorted(options.items())]}
        canonical = json.dumps(obj, sort_keys=True, ensure_ascii=True, separators=(',', ':'), allow_nan=False)
        cases.append({'name': name, 'canonical': canonical,
                      'sha256': hashlib.sha256(canonical.encode('ascii')).hexdigest()})
    return cases


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true', help='Create a new fixture; refuse overwriting.')
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[2] / 'tests/nonplanar/data/config-fingerprint-v1.json'
    expected = fixtures()
    if args.write:
        with path.open('x') as output:
            json.dump(expected, output, indent=2)
            output.write('\n')
    if json.loads(path.read_text()) != expected:
        parser.exit(1, 'Canonical config fixture disagrees with independent oracle\n')
    print(json.dumps({'status': 'PASS', 'cases': len(expected), 'sha256': [c['sha256'] for c in expected]}))
