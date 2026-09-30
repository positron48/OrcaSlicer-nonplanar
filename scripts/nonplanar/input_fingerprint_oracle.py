#!/usr/bin/env python3
"""Independent exact native mesh and pre-apply input identity vectors."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def fixtures():
    meshes = []
    for name, vertices, indices, properties in [
        ('empty', [], [], []),
        ('triangle', [(0.1, -0.0, 1.0), (2.0, 0.0, 1.0), (0.0, 2.0, 1.0)], [(0, 1, 2)], [(0, 0.125)]),
    ]:
        raw = b'nptop-native-mesh-v1\0' + struct.pack('>Q', len(vertices))
        raw += b''.join(struct.pack('>fff', *v) for v in vertices)
        raw += struct.pack('>Q', len(indices)) + b''.join(struct.pack('>iii', *t) for t in indices)
        raw += struct.pack('>Q', len(properties)) + b''.join(struct.pack('>id', *p) for p in properties)
        meshes.append({'name': name, 'sha256': hashlib.sha256(raw).hexdigest()})
    obj = {'config': {'options': [[b'nptop_mode'.hex(), 3, False, b'safe_hybrid'.hex()]], 'schema': 1},
           'materials': [], 'model_plate_index': 0, 'objects': [], 'plate_actions': [], 'schema': 1}
    canonical = json.dumps(obj, sort_keys=True, separators=(',', ':'), allow_nan=False)
    return {'meshes': meshes, 'input': {'canonical': canonical, 'sha256': hashlib.sha256(canonical.encode()).hexdigest()}}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    path = Path(__file__).resolve().parents[2] / 'tests/nonplanar/data/input-fingerprint-v1.json'
    expected = fixtures()
    if args.write:
        with path.open('x') as output:
            json.dump(expected, output, indent=2)
            output.write('\n')
    if json.loads(path.read_text()) != expected:
        parser.exit(1, 'Native input fixtures disagree with independent oracle\n')
    print(json.dumps({'status': 'PASS', 'mesh_cases': len(expected['meshes']), 'input_cases': 1}))
