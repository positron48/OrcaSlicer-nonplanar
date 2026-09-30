#!/usr/bin/env python3
"""Independent body encoding vector only; synthetic data never enters slicing."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from partition_fingerprint_oracle import fixture as partition_fixture
from print_config_fingerprint_oracle import fixtures as print_fixtures


def fixture():
    bits = lambda value: struct.pack('>d', value).hex()
    string = lambda value: value.encode('ascii').hex()
    canonical = lambda value: json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False)
    empty = hashlib.sha256(canonical({'options': [], 'schema': 1}).encode('ascii')).hexdigest()
    entities = [[0, 0, True, False, -1, None], [0, 0, False, True, 0, None], [2, 3, False, False, 0, None]]
    path = [3, 5, bits(.4), bits(.2), bits(.08), bits(.01), [bits(1), bits(1)], [bits(.08), bits(.08)],
            [[0, 0, 0], [1000000, 0, 0]], [[bits(20), bits(-0.), bits(3.6)], [bits(21), bits(-0.), bits(3.6)]]]
    region = [7, string(empty), 71, 2, bits(.2), bits(.6), [bits(20), bits(-0.), bits(3)], bits(1e-6),
              entities, [path], [bits(.08), bits(.08)]]
    obj = {'engine_non_bbl': True, 'executed_full': string(empty), 'executed_object': string(empty),
           'executed_print': string(empty), 'guarded_settings': string(print_fixtures()[0]['sha256']),
           'native_scale': bits(1e-6), 'origin_error': bits(.1), 'partition': string(partition_fixture()['sha256']),
           'regions': [region], 'revision': 71, 'schema': 1, 'volume': [bits(.08), bits(.08)]}
    encoded = canonical(obj)
    return {'canonical': encoded, 'sha256': hashlib.sha256(encoded.encode('ascii')).hexdigest()}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[2] / 'tests/nonplanar/data/planar-body-fingerprint-v1.json'
    expected = fixture()
    if args.write:
        with path.open('x') as output:
            json.dump(expected, output, indent=2)
            output.write('\n')
    if json.loads(path.read_text()) != expected:
        parser.exit(1, 'Body identity disagrees with independent oracle\n')
    print(json.dumps({'status': 'PASS', 'cases': 1, 'sha256': expected['sha256']}))
