#!/usr/bin/env python3
"""Independent report/manifest byte identity and complete registry oracle.

This checks software evidence, not motion mathematics, geometry, resource
qualification, authenticity or permission to publish a job.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct

REGISTRY = [
    'actual_support', 'candidate_manifest_identity', 'complete_route_order',
    'config_compatibility', 'continuous_geometry', 'final_filters',
    'final_material', 'final_rates', 'firmware_preconditions',
    'independent_replay', 'material_delivery_qualification',
    'numeric_algorithm_qualification', 'profile_complete', 'scene_coverage',
    'software_identity', 'source_model_plan_binding', 'target_volume_seam',
]
IMPLEMENTED = {'candidate_manifest_identity', 'final_material', 'final_rates', 'independent_replay'}


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def unique_object(pairs):
    value = {}
    for key, item in pairs:
        require(key not in value, 'Duplicate JSON key: ' + key)
        value[key] = item
    return value


def reject_constant(value):
    raise ValueError('Nonfinite JSON constant: ' + value)


def finite_float(value):
    number = float(value)
    require(math.isfinite(number), 'Nonfinite JSON number')
    return number


def parse(text):
    return json.loads(text, object_pairs_hook=unique_object, parse_constant=reject_constant, parse_float=finite_float)


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=False, allow_nan=False)


def sha(text):
    return hashlib.sha256(text.encode('utf-8')).hexdigest()


def verify(record):
    for key in ['sha256', 'manifest_sha256', 'candidate_sha256', 'job_fingerprint', 'material_journal',
                'motion_policy', 'serializer_policy', 'source_fingerprint']:
        require(type(record[key]) is str and re.fullmatch('[a-f0-9]{64}', record[key]), 'Invalid hash: ' + key)
    for key in ['attempt', 'job_id', 'job_revision', 'source_revision']:
        require(type(record[key]) is int and 0 < record[key] <= 2**64 - 1, 'Invalid integer: ' + key)
    require(len(record['initial_position']) == 3 and
            all(type(value) in {int, float} and math.isfinite(value) for value in record['initial_position']), 'Invalid initial pose')
    document = parse(record['canonical'])
    require(canonical(document) == record['canonical'], 'Noncanonical report')
    require(sha(record['canonical']) == record['sha256'], 'Report SHA mismatch')
    require(set(document) == {'schema', 'registry_version', 'scope', 'attempt', 'job_fingerprint',
                             'manifest_sha256', 'replay', 'validation'}, 'Report fields')
    require(type(document['schema']) is int and document['schema'] == 1 and
            type(document['registry_version']) is int and document['registry_version'] == 1, 'Report versions')
    require(document['scope'] == 'declared_linear_final_byte_replay_incomplete_job', 'Report scope')
    require(sha(record['candidate_bytes']) == record['candidate_sha256'], 'Candidate SHA mismatch')
    require(sha(record['job_canonical']) == record['job_fingerprint'], 'Job SHA mismatch')
    job = parse(record['job_canonical'])
    require(job['job_id'] == record['job_id'] and job['native_revision'] == record['job_revision'], 'Job identity')
    encode = lambda value: value.encode('utf-8').hex()
    manifest = {
        'attempt': record['attempt'], 'candidate_sha256': encode(record['candidate_sha256']),
        'candidate_size': len(record['candidate_bytes'].encode('utf-8')),
        'initial_position': [struct.pack('>d', value).hex() for value in record['initial_position']],
        'job_fingerprint': encode(record['job_fingerprint']), 'job_id': record['job_id'],
        'material_journal': encode(record['material_journal']), 'motion_policy': encode(record['motion_policy']),
        'schema': 1, 'scope': 'job_context_candidate_byte_identity_only',
        'serializer_policy': encode(record['serializer_policy']), 'source_fingerprint': encode(record['source_fingerprint']),
        'source_revision': record['source_revision'],
    }
    require(parse(record['manifest']) == manifest and canonical(manifest) == record['manifest'], 'Manifest binding')
    require(sha(record['manifest']) == record['manifest_sha256'] == document['manifest_sha256'], 'Manifest SHA mismatch')
    require(document['job_fingerprint'] == record['job_fingerprint'] and
            document['attempt'] == record['attempt'], 'Report attempt binding')
    validation = document['validation']
    require(set(validation) == {'schema_version', 'document_example', 'job_id', 'job_revision', 'overall_status',
                                'export_decision', 'gcode_sha256', 'mandatory_check_ids', 'checks', 'assumptions'}, 'Validation fields')
    require(validation['schema_version'] == '0.1.0-draft', 'Validation version')
    require(type(validation['job_revision']) is int and validation['job_revision'] > 0, 'Validation revision')
    require(type(validation['assumptions']) is list and
            all(type(value) is str for value in validation['assumptions']), 'Validation assumptions')
    require(validation['document_example'] is False, 'Example is not actual evidence')
    require(validation['job_id'] == str(record['job_id']) and validation['job_revision'] == record['job_revision'], 'Report job binding')
    require(validation['gcode_sha256'] == record['candidate_sha256'], 'Report candidate binding')
    require(validation['mandatory_check_ids'] == REGISTRY, 'Incomplete mandatory registry')
    checks = validation['checks']
    require([check['id'] for check in checks] == REGISTRY, 'Missing duplicate reordered or unknown checks')
    for check in checks:
        require(set(check) == {'id', 'mandatory', 'status', 'execution', 'reason'} and
                check['status'] in {'PASS', 'FAIL', 'UNKNOWN'} and check['execution'] in {'RUN', 'NOT_RUN', 'SKIPPED', 'ERROR'} and
                type(check['reason']) is str, 'Check shape')
        require(check['mandatory'] is True and bool(check['reason']), 'Mandatory flag or missing reason')
        require(check['status'] != 'PASS' or check['execution'] == 'RUN', 'Unexecuted PASS')
        if check['id'] not in IMPLEMENTED:
            require(check['status'] == 'UNKNOWN' and check['execution'] == 'NOT_RUN', 'Unimplemented proof claim')
    indexed = {check['id']: check for check in checks}
    require(indexed['candidate_manifest_identity']['status'] == 'PASS' and
            indexed['candidate_manifest_identity']['execution'] == 'RUN', 'Manifest check')
    replay = document['replay']
    require(set(replay) == {'records', 'evaluations', 'rate_status', 'rate_reason', 'material_status', 'material_reason'}, 'Replay fields')
    for name in ['rates', 'material']:
        prefix = 'rate' if name == 'rates' else name
        require(indexed['final_' + name]['status'] == replay[prefix + '_status'] and
                indexed['final_' + name]['reason'] == replay[prefix + '_reason'], 'Replay check binding')
    require(indexed['final_rates']['execution'] == 'RUN', 'Rates execution')
    require(indexed['independent_replay']['status'] == indexed['final_material']['status'] and
            indexed['independent_replay']['execution'] == indexed['final_material']['execution'], 'Journal execution')
    require(type(replay['evaluations']) is int and replay['evaluations'] == record['evaluations'] and replay['evaluations'] > 0, 'Work accounting')
    require(type(replay['records']) is int and replay['records'] == record['records'], 'Replay count')
    if replay['rate_status'] == 'PASS':
        count = sum(line.startswith(('G1 ', 'G4 ')) for line in record['candidate_bytes'].splitlines())
        require(count == replay['records'] and count > 0, 'Actual byte record count')
        require(indexed['final_material']['execution'] == 'RUN', 'Material execution')
    else:
        require(replay['records'] == 0 and indexed['final_material']['execution'] == 'SKIPPED', 'Skipped material')
    overall = 'FAIL' if any(check['status'] == 'FAIL' for check in checks) else 'UNKNOWN'
    require(validation['overall_status'] == overall and validation['export_decision'] == 'BLOCK', 'Incomplete report cannot allow export')
    return {'status': 'PASS', 'scope': 'EXACT_REPORT_MANIFEST_BYTE_IDENTITY_AND_FIXED_REGISTRY_ONLY',
            'mandatory': len(checks), 'records': replay['records'], 'job_status': overall, 'export': 'BLOCK'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(parse(args.input.read_text()))))


if __name__ == '__main__':
    main()
