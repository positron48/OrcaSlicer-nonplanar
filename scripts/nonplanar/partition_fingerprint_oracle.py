#!/usr/bin/env python3
"""Independent partition identity vector; synthetic encoding data, no geometry PASS."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def fixture():
    empty = hashlib.sha256(b'nptop-native-mesh-v1\0' + struct.pack('>QQQ', 0, 0, 0)).hexdigest()
    string = lambda value: value.encode('ascii').hex()
    bits = lambda value: struct.pack('>d', value).hex()
    obj = {'body': string(empty), 'cap': string(empty), 'input': string('input'),
           'original': string(empty), 'partition_error': bits(0.1), 'plate_index': 2,
           'plate_origin': [bits(0.1), bits(-0.0), bits(2.0)], 'reservation': string(empty),
           'revision': 19, 'schema': 1, 'source': string('source'), 'total_error': bits(0.2)}
    canonical = json.dumps(obj, sort_keys=True, separators=(',', ':'), allow_nan=False)
    return {'canonical': canonical, 'sha256': hashlib.sha256(canonical.encode('ascii')).hexdigest()}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[2] / 'tests/nonplanar/data/partition-fingerprint-v1.json'
    expected = fixture()
    if args.write:
        with path.open('x') as output:
            json.dump(expected, output, indent=2)
            output.write('\n')
    if json.loads(path.read_text()) != expected:
        parser.exit(1, 'Partition identity disagrees with independent oracle\n')
    print(json.dumps({'status': 'PASS', 'cases': 1, 'sha256': expected['sha256']}))
