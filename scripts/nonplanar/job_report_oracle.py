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


def native_lineage(record, job):
    """Exact dependency identity only; no geometry or material math approval."""
    native = record['native']
    for prefix in ['', 'hatch_', 'body_']:
        text, digest = native[prefix + 'canonical'], native[prefix + 'sha256']
        require(type(digest) is str and re.fullmatch('[a-f0-9]{64}', digest), 'Native digest')
        require(sha(text) == digest and canonical(parse(text)) == text, 'Native canonical identity')
    plan, hatch, body = (parse(native[key]) for key in ['canonical', 'hatch_canonical', 'body_canonical'])
    require(set(plan) == {'schema', 'scope', 'job_fingerprint', 'attempt', 'hatch_lineage',
                          'assembled_journal', 'planned_journal', 'candidate_sha256'}, 'Native plan fields')
    require(set(hatch) == {'schema', 'scope', 'body_lineage', 'request', 'geometry'}, 'Native hatch fields')
    require(set(body) == {'schema', 'scope', 'job_fingerprint', 'attempt', 'source_sha256',
                          'slicing_input', 'request', 'body', 'body_material', 'body_journal'}, 'Native body fields')
    for document, scope in [(plan, 'owned_native_body_cap_linear_candidate_lineage_only'),
                            (hatch, 'owned_native_affine_hatch_dependency_lineage_only'),
                            (body, 'owned_native_body_dependency_lineage_only')]:
        require(type(document['schema']) is int and document['schema'] == 1 and document['scope'] == scope, 'Native version/scope')
    for document in [plan, body]:
        require(type(document['attempt']) is int and document['attempt'] == record['attempt'] and
                document['job_fingerprint'] == record['job_fingerprint'], 'Native owner/attempt')
    require(plan['hatch_lineage'] == native['hatch_sha256'] and hatch['body_lineage'] == native['body_sha256'], 'Native ancestry')
    require(body['slicing_input'].encode().hex() == job['native_input'] and body['request']['millimeters_declared'] is True, 'Native source units/input')
    require(any(type(resource[0]) is int and resource[0] == 0 and resource[2] == body['source_sha256'].encode().hex()
                for resource in job['resources']), 'Native original source bytes')
    require(plan['candidate_sha256'] == record['candidate_sha256'] and plan['planned_journal'] == record['material_journal'], 'Native candidate/journal')
    journals = {}
    for name in ['assembled', 'planned', 'body']:
        journal = native[name]
        require(set(journal) == {'context', 'records', 'sha256'}, 'Native journal fields')
        context = parse(journal['context'])
        require(canonical(context) == journal['context'] and type(context['record_count']) is int and
                context['record_count'] == len(journal['records']) and type(context['revision']) is int and
                context['revision'] == record['job_revision'], 'Native journal context')
        digest = sha('nptop-material-ledger-v1\0' + journal['context'])
        rows = []
        for text in journal['records']:
            row = parse(text)
            require(canonical(row) == text, 'Native row canonical identity')
            digest = sha('nptop-material-record-v1\0' + digest + text)
            rows.append(row)
        require(digest == journal['sha256'], 'Native ledger SHA')
        journals[name] = (context, rows)
    require(native['body']['sha256'] == body['body_journal'] and native['assembled']['sha256'] == plan['assembled_journal'] and
            native['planned']['sha256'] == plan['planned_journal'], 'Native ledger dependency')
    require(journals['body'][0]['source'] == body['body'].encode().hex(), 'Native body geometry journal binding')
    original_context, original = journals['assembled']
    planned_context, planned = journals['planned']
    require(original_context == planned_context and len(original) == len(planned), 'Native planned context')
    require(original[:len(journals['body'][1])] == journals['body'][1] and
            len(original) > len(journals['body'][1]), 'Complete original body plus cap')
    for a, b in zip(original, planned):
        # Binary64 bit strings encode nonnegative finite quantities here.
        # Decode before checking reductions; all other fields must be exact.
        adjusted = parse(canonical(a))
        for index in [5, 6]:
            before, after = (struct.unpack('>d', bytes.fromhex(row['motion'][index]))[0] for row in [a, b])
            require(math.isfinite(before) and math.isfinite(after) and 0 < after <= before, 'Native motion limit reduction')
            adjusted['motion'][index] = b['motion'][index]
        require(adjusted == b, 'Native planner changed geometry/order/material')
    return native['sha256']


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
    if 'native' in record:
        manifest['schema'] = 2
        manifest['scope'] = 'owned_native_body_cap_candidate_lineage_only'
        manifest['native_lineage'] = encode(native_lineage(record, job))
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
