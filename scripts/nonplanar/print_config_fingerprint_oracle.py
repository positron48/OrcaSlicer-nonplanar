#!/usr/bin/env python3
"""Independent settings-only Print snapshot identity vectors; no approval claim."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def fixtures():
    bits = lambda value: struct.pack('>d', value).hex()
    empty = {'options': [], 'schema': 1}
    mixed = {'options': [['61', 1, False, bits(0.1)]], 'schema': 1}
    cases = []
    for name, full, regions, conflict in [
        ('empty', empty, [], None),
        ('mixed', mixed, [[0, 7, empty], [0, 9, mixed]],
         [b'key'.hex(), b'value'.hex(), b'reason'.hex()]),
    ]:
        obj = {'full_config': full, 'input_conflict_hex': b'blocked\0reason'.hex(),
               'instance_count': 1, 'model_conflict': conflict, 'object_count': 1,
               'plate_index': 3, 'plate_origin_mm': [bits(0.1), bits(-0.0), bits(2.0)],
               'regions': regions, 'resolved_print_config': full, 'schema': 1}
        canonical = json.dumps(obj, sort_keys=True, ensure_ascii=True, separators=(',', ':'), allow_nan=False)
        cases.append({'name': name, 'canonical': canonical,
                      'sha256': hashlib.sha256(canonical.encode('ascii')).hexdigest()})
    return cases


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true', help='Create new fixtures without overwriting.')
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[2] / 'tests/nonplanar/data/print-config-fingerprint-v1.json'
    expected = fixtures()
    if args.write:
        with path.open('x') as output:
            json.dump(expected, output, indent=2)
            output.write('\n')
    if json.loads(path.read_text()) != expected:
        parser.exit(1, 'Print config fixtures disagree with the independent oracle\n')
    print(json.dumps({'status': 'PASS', 'cases': len(expected), 'sha256': [c['sha256'] for c in expected]}))
