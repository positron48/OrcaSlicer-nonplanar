#!/usr/bin/env python3
"""Independent encoding vector for native body/material source associations."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from planar_body_fingerprint_oracle import fixture as body_fixture
from material_fingerprint_oracle import fixture as material_fixture


def fixture():
    number = lambda value: struct.pack('>d', value).hex()
    canonical = lambda value: json.dumps(value, sort_keys=True, separators=(',', ':'), allow_nan=False)
    context = canonical({'body': body_fixture()['sha256'].encode().hex(),
                         'material': material_fixture()['sha256'].encode().hex(),
                         'plate_origin': list(map(number, (.1, -.2, 1))), 'reference_count': 2, 'schema': 1})
    references = [canonical([0, 0, 0, 0, number(.01), number(.02)]),
                  canonical([3, 2, 3, 4, number(1e-6), number(-0.)])]
    digest = hashlib.sha256(b'nptop-body-material-v1\0' + context.encode('ascii')).hexdigest()
    for reference in references:
        digest = hashlib.sha256(b'nptop-body-bead-v1\0' + digest.encode('ascii') + reference.encode('ascii')).hexdigest()
    return {'context': context, 'references': references, 'sha256': digest}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[2] / 'tests/nonplanar/data/body-material-fingerprint-v1.json'
    expected = fixture()
    if args.write:
        with path.open('x') as output:
            output.write(json.dumps(expected, indent=2) + '\n')
    if json.loads(path.read_text()) != expected:
        parser.exit(1, 'Native material identity disagrees with independent oracle\n')
    print(json.dumps({'status': 'PASS', 'sha256': expected['sha256']}))
