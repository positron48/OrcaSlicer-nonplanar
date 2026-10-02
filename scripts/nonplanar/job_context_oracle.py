#!/usr/bin/env python3
"""Independent SHA256 and exact canonical JSON oracle for native job contexts."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input', type=Path, required=True)
args = parser.parse_args()
record = json.loads(args.input.read_text())
sha = lambda value: hashlib.sha256(value.encode('utf-8')).hexdigest()
encode = lambda value: value.encode('utf-8').hex()
transport = {encode(key) for key in (
    'printer_agent', 'print_host', 'print_host_webui', 'printhost_apikey',
    'flashforge_serial_number', 'printhost_port', 'printhost_cafile', 'printhost_user',
    'printhost_password', 'printhost_ssl_ignore_revoke', 'printhost_authorization_type', 'timestamp', 'logfile')}
def identity(value):
    if isinstance(value, dict):
        if set(value) == {'options', 'schema'} and value['schema'] == 1:
            return {'options': [option for option in value['options'] if option[0] not in transport], 'schema': 1}
        return {key: identity(child) for key, child in value.items()}
    if isinstance(value, list):
        return [identity(child) for child in value]
    return value
for prefix in ('native_input', 'executed_input', 'print_settings'):
    assert sha(record[prefix + '_canonical']) == record[prefix + '_fingerprint'], prefix
    view = identity(json.loads(record[prefix + '_canonical']))
    if prefix == 'print_settings':
        view['input_fingerprint'] = encode(record['native_input_identity_fingerprint'])
    canonical_view = json.dumps(view, sort_keys=True, separators=(',', ':'), allow_nan=False)
    assert canonical_view == record[prefix + '_identity']
    assert sha(canonical_view) == record[prefix + '_identity_fingerprint']
resources = sorted(record['resources'], key=lambda r: (r['kind'], r['name'].encode('utf-8')))
assert {r['kind'] for r in resources if r['kind'] != 0} == set(range(1, 8))
assert len({(r['kind'], r['name']) for r in resources}) == len(resources)
assert len([r for r in resources if r['kind'] != 0]) == 7
for resource in resources:
    assert resource['sha256'] == sha(resource['bytes'])
expected = {
    'executed_input': encode(record['executed_input_identity_fingerprint']),
    'job_id': record['job_id'],
    'native_input': encode(record['native_input_identity_fingerprint']),
    'native_revision': record['input_revision'],
    'print_settings': encode(record['print_settings_identity_fingerprint']),
    'resources': [[r['kind'], encode(r['name']), encode(sha(r['bytes'])), len(r['bytes'].encode('utf-8'))]
                  for r in resources],
    'schema': 1,
}
canonical = json.dumps(expected, sort_keys=True, separators=(',', ':'), allow_nan=False)
assert canonical == record['canonical']
assert sha(canonical) == record['fingerprint']
# No geometry, measured profile, finished verification or publication claim.
print(json.dumps({'status': 'PASS', 'resources': len(resources), 'job_id': record['job_id'],
                  'scope': 'EXACT_JOB_INPUT_RESOURCE_CANONICAL_HASH_ONLY', 'export': 'BLOCK'}))
