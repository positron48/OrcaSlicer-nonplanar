#!/usr/bin/env python3
"""Independent synthetic material ledger and prefix byte/hash vector."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def fixture():
    number = lambda value: struct.pack('>d', value).hex()
    canonical = lambda value: json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False)
    position = lambda x, y, z: list(map(number, (x, y, z)))
    source = 'a' * 64
    context = canonical({'model': [17] + list(map(number, (.01, .01, .01, .01, 0))),
                         'record_count': 4, 'revision': 23, 'schema': 1, 'source': source.encode().hex()})
    records = [canonical({'bead': [0, number(.2), number(.4), list(map(number, (.749999, 1.500001)))],
                          'geometry': [[number(10), number(10)], [number(3), number(3)]],
                          'motion': [101, 0, 33, position(0, -0., 1), position(10, 0, 2), number(20), number(100),
                                     [1] + list(map(number, (3, 1, .2, .4))) + [1, 2, 3, 7, 8], 4]}),
               canonical({'bead': None, 'geometry': None,
                          'motion': [102, 1, 0, position(10, 0, 2), position(20, 0, 2), number(10), number(100), [0], -1]})]
    for event, before, after in ((103, 0, 1), (104, 1, 0)):
        records.append(canonical({'bead': None, 'geometry': None,
                                  'motion': [event, event - 101, 0, position(20, 0, 2), position(20, 0, 2),
                                             number(10), number(100), [2, number(.8), before, after], -1]}))
    digest = hashlib.sha256(b'nptop-material-ledger-v1\0' + context.encode('ascii')).hexdigest()
    for record in records:
        digest = hashlib.sha256(b'nptop-material-record-v1\0' + digest.encode('ascii') + record.encode('ascii')).hexdigest()
    prefix = canonical({'completed_records': 0, 'current_progress': number(.4),
                        'nominal_deposited_volume': list(map(number, (1.19, 1.21))),
                        'schema': 1, 'sequence': digest.encode().hex()})
    return {'context': context, 'records': records, 'sha256': digest,
            'prefix': {'canonical': prefix, 'sha256': hashlib.sha256(b'nptop-material-prefix-v1\0' + prefix.encode('ascii')).hexdigest()}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[2] / 'tests/nonplanar/data/material-fingerprint-v1.json'
    expected = fixture()
    if args.write:
        with path.open('x') as output:
            output.write(json.dumps(expected, indent=2) + '\n')
    elif json.loads(path.read_text()) != expected:
        parser.exit(1, 'Material identity disagrees with independent oracle\n')
    print(json.dumps({'status': 'PASS', 'sha256': expected['sha256'], 'prefix_sha256': expected['prefix']['sha256']}))


if __name__ == '__main__':
    main()
