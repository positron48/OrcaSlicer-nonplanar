#!/usr/bin/env python3
"""Independent exact job/candidate/initial-pose manifest binding oracle."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input', type=Path, required=True)
args = parser.parse_args()
record = json.loads(args.input.read_text())
sha = lambda text: hashlib.sha256(text.encode('utf-8')).hexdigest()
encode = lambda text: text.encode('utf-8').hex()
assert sha(record['candidate_bytes']) == record['candidate_sha256']
assert sha(record['job_canonical']) == record['job_fingerprint']
expected = {
    'attempt': record['attempt'],
    'candidate_sha256': encode(sha(record['candidate_bytes'])),
    'candidate_size': len(record['candidate_bytes'].encode('utf-8')),
    'initial_position': [struct.pack('>d', v).hex() for v in record['initial_position']],
    'job_fingerprint': encode(sha(record['job_canonical'])),
    'job_id': record['job_id'],
    'material_journal': encode(record['material_journal']),
    'motion_policy': encode(record['motion_policy']),
    'schema': 1,
    'scope': 'job_context_candidate_byte_identity_only',
    'serializer_policy': encode(record['serializer_policy']),
    'source_fingerprint': encode(record['source_fingerprint']),
    'source_revision': record['source_revision'],
}
canonical = json.dumps(expected, sort_keys=True, separators=(',', ':'), allow_nan=False)
assert canonical == record['manifest']
assert sha(canonical) == record['manifest_sha256']
assert json.loads(record['job_canonical'])['job_id'] == record['job_id']
print(json.dumps({'status': 'PASS', 'candidate_bytes': expected['candidate_size'],
                  'scope': 'EXACT_BYTE_INITIAL_POSE_DEPENDENCY_MANIFEST_BINDING_ONLY', 'export': 'BLOCK'}))
