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


def departure_lineage(native, plan, journals):
    """Identity/order/request relation only; no clearance or whole-job approval."""
    text, digest = native['departure_canonical'], native['departure_sha256']
    require(type(digest) is str and re.fullmatch('[a-f0-9]{64}', digest), 'Departure digest')
    require(sha(text) == digest == plan['departure_lineage'] and canonical(parse(text)) == text, 'Departure identity')
    departure = parse(text)
    require(set(departure) == {'schema', 'scope', 'before_journal', 'laid_journal', 'routed_journal',
                              'clearance', 'scene', 'request', 'legs', 'source_records'}, 'Departure fields')
    require(type(departure['schema']) is int and departure['schema'] == 1 and
            departure['scope'] == 'owned_simulation_cap_departure_lineage_only', 'Departure version/scope')

    def number(value):
        require(type(value) is str and re.fullmatch('[a-f0-9]{16}', value), 'Departure binary64')
        result = struct.unpack('>d', bytes.fromhex(value))[0]
        require(math.isfinite(result), 'Departure finite quantity')
        return result

    def position(value):
        require(type(value) is list and len(value) == 3, 'Departure position')
        return [number(v) for v in value]

    def box(value):
        require(type(value) is list and len(value) == 2, 'Departure box')
        lo, hi = (position(v) for v in value)
        require(all(a <= b for a, b in zip(lo, hi)), 'Departure box order')

    for field, name in [('before_journal', 'before'), ('laid_journal', 'assembled'), ('routed_journal', 'routed')]:
        require(departure[field] == native[name]['sha256'].encode().hex(), 'Departure journal edge')
    before_context, before = journals['before']
    laid_context, laid = journals['assembled']
    routed_context, routed = journals['routed']
    require(before[:len(journals['body'][1])] == journals['body'][1] and len(before) > len(journals['body'][1]), 'Departure complete original body/cap')
    require(laid[:len(before)] == before and len(laid) > len(before), 'Departure preserves complete parent before prospective bead')
    require(routed[:len(laid)] == laid, 'Departure preserves every laid row')
    require(all(before_context[k] == laid_context[k] == routed_context[k] for k in ['source', 'revision', 'schema']), 'Departure source context')
    require(before_context['model'][:5] == laid_context['model'][:5] and
            number(laid_context['model'][5]) >= number(before_context['model'][5]), 'Departure preserves model/losses/error budget')
    require({k: v for k, v in laid_context.items() if k != 'record_count'} ==
            {k: v for k, v in routed_context.items() if k != 'record_count'}, 'Departure routed context')
    request = departure['request']
    require(type(request) is list and len(request) == 4, 'Departure request')
    target = position(request[0]);height, speed, acceleration = (number(v) for v in request[1:])
    start = laid[-1]['motion'][4];start_values = position(start)
    require(height >= max(start_values[2], target[2]) and speed > 0 and acceleration > 0, 'Departure height/motion domain')
    points = [start]
    for p in [[start[0], start[1], request[1]], [request[0][0], request[0][1], request[1]], request[0]]:
        if position(points[-1]) != position(p):
            points.append(p)
    if len(points) == 1:
        points.append(request[0])  # original instant Travel is still an obligation
    legs = len(points)-1
    require(1 <= legs <= 3 and len(routed) == len(laid)+legs, 'Departure complete route length')
    require(departure['source_records'] == list(range(len(laid)))+[len(laid)]*legs and
            all(type(v) is int for v in departure['source_records']), 'Departure exact origin map')
    maximum = max(row['motion'][0] for row in laid)
    for i, (a, b) in enumerate(zip(points, points[1:])):
        expected = {'bead': None, 'geometry': None,
                    'motion': [maximum+1+i, len(laid)+i, 0, a, b, request[2], request[3], [0], -1]}
        require(routed[len(laid)+i] == expected, 'Departure exact Travel poses IDs order and payload')
    require(type(departure['legs']) is list and len(departure['legs']) == legs and
            all(type(v) is list and len(v) == 2 and type(v[0]) is int and v[0] == len(laid)+i and
                type(v[1]) is int and 0 <= v[1] <= 1000000 for i, v in enumerate(departure['legs'])), 'Departure leg obligations')
    require(type(departure['clearance']) is list and len(departure['clearance']) == 9 and
            all(number(v) >= 0 for v in departure['clearance']), 'Departure clearance policy')
    scene = departure['scene']
    require(set(scene) == {'coverage', 'head', 'identity', 'obstacles', 'tip'}, 'Departure whole scene fields')
    identity = scene['identity']
    require(type(identity) is list and len(identity) == 5 and all(type(v) is int for v in identity[:4]) and
            identity[0] == 1 and identity[1] > 0 and identity[2] > 0 and identity[3] == 0 and identity[4] is False, 'Departure simulation scene identity')
    coverage = scene['coverage']
    require(type(coverage) is list and len(coverage) == 5 and coverage[3] is True and
            number(coverage[2]) >= 0 and number(coverage[4]) >= 0, 'Departure complete inventory/uncertainty')
    box(coverage[0]);box(coverage[1])
    require(type(scene['head']) is list and 6 <= len(scene['head']) <= 64, 'Departure complete head')
    ids, roles = set(), set()
    for part in scene['head']:
        require(type(part) is list and len(part) == 5 and type(part[0]) is int and part[0] > 0 and part[0] not in ids and
                type(part[1]) is int and 0 <= part[1] < 6 and type(part[3]) is bool and type(part[4]) is bool and
                (not part[3] or part[4]), 'Departure head identity/coverage')
        ids.add(part[0]);roles.add(part[1]);box(part[2])
    require(roles == set(range(6)), 'Departure all head roles')
    require(type(scene['obstacles']) is list and len(scene['obstacles']) <= 10000, 'Departure bounded obstacle inventory')
    for obstacle in scene['obstacles']:
        box(obstacle)
    tip = scene['tip']
    require(type(tip) is list and len(tip) == 3, 'Departure complete annulus')
    position(tip[0]);require(0 < number(tip[1]) < number(tip[2]), 'Departure opening/outer radius')
    return routed_context, routed


def decoded_number(value):
    require(type(value) is str and re.fullmatch('[a-f0-9]{16}', value), 'Later binary64')
    result = struct.unpack('>d', bytes.fromhex(value))[0]
    require(math.isfinite(result), 'Later finite quantity')
    return result


def decoded_journal(journal, revision):
    require(set(journal) == {'context', 'records', 'sha256'}, 'Native journal fields')
    context = parse(journal['context'])
    require(canonical(context) == journal['context'] and type(context['record_count']) is int and
            context['record_count'] == len(journal['records']) and type(context['revision']) is int and
            context['revision'] == revision, 'Native journal context')
    digest = sha('nptop-material-ledger-v1\0' + journal['context'])
    rows = []
    for text in journal['records']:
        row = parse(text)
        require(canonical(row) == text, 'Native row canonical identity')
        digest = sha('nptop-material-record-v1\0' + digest + text)
        rows.append(row)
    require(digest == journal['sha256'], 'Native ledger SHA')
    return context, rows


def later_lineage(native, plan, journals, revision):
    """Independent exact prefix/request/append order; no physical approval."""
    text, digest = native['later_canonical'], native['later_sha256']
    require(sha(text) == digest == plan['later_lineage'] and canonical(parse(text)) == text, 'Later identity')
    later = parse(text)
    require(set(later) == {'schema', 'scope', 'before_journal', 'after_journal', 'paths', 'requests'} and
            type(later['schema']) is int and later['schema'] == 1 and
            later['scope'] == 'owned_ordered_later_path_lineage_only', 'Later registry/version/scope')
    paths, requests, prefixes = later['paths'], later['requests'], native['later_prefixes']
    require(type(paths) is list and type(requests) is list and type(prefixes) is list and
            0 < len(paths) == len(requests) == len(prefixes) <= 4096, 'Later complete program')
    require(later['before_journal'] == prefixes[0]['sha256'].encode().hex() and
            later['after_journal'] == native['assembled']['sha256'].encode().hex(), 'Later journal edges')
    decoded = [decoded_journal(p, revision) for p in prefixes] + [journals['assembled']]
    require(decoded[0][1][:len(journals['body'][1])] == journals['body'][1] and
            len(decoded[0][1]) > len(journals['body'][1]), 'Later complete original body/cap')
    prior_pass = 0
    hatch = parse(native['hatch_canonical'])['request']
    width, first_direction = decoded_number(hatch['hatch'][0]), hatch['hatch'][3]
    for i, (path, request) in enumerate(zip(paths, requests)):
        require(type(request) is list and len(request) == 6 and type(request[0]) is int and
                0 < request[0] < hatch['passes'][0] and prior_pass <= request[0] <= prior_pass+1, 'Later original pass order')
        prior_pass = request[0]
        lo_x, lo_y, hi_x, hi_y, plane = map(decoded_number, request[1:])
        require(lo_x < hi_x and lo_y < hi_y, 'Later finite footprint')
        require(type(path) is list and len(path) == 5 and all(type(v) is int for v in path[:3]), 'Later range types')
        before_context, before = decoded[i]
        after_context, after = decoded[i+1]
        require(path == [request[0], len(before), len(after)-len(before), prefixes[i]['sha256'].encode().hex(),
                         (prefixes[i+1] if i+1 < len(prefixes) else native['assembled'])['sha256'].encode().hex()], 'Later exact adjacent prefix ranges')
        require(len(after) > len(before) and after[:len(before)] == before, 'Later retains every actual previous row')
        require(all(before_context[k] == after_context[k] for k in ['source', 'revision', 'schema']) and
                before_context['model'][:5] == after_context['model'][:5] and
                decoded_number(after_context['model'][5]) >= decoded_number(before_context['model'][5]), 'Later preserves source/losses/error budget')
        added = after[len(before):]
        deposits = [row for row in added if row['bead'] is not None]
        require(deposits and all(row['motion'][2] in [0, 1] for row in added), 'Later explicit connector/deposition only')
        direction = (first_direction + request[0]) % 2
        for row in deposits:
            motion = row['motion'];a, b = ([decoded_number(v) for v in motion[j]] for j in [3, 4])
            require(decoded_number(motion[7][2]) == width and decoded_number(motion[7][1]) > 0, 'Later original fixed width/positive volume')
            require(all(lo_x <= p[0] <= hi_x and lo_y <= p[1] <= hi_y for p in [a, b]), 'Later actual axis remains in requested footprint')
            require((a[1] == b[1] and a[0] < b[0]) if direction == 0 else (a[0] == b[0] and a[1] < b[1]), 'Later original alternating direction')
        maximum = max(row['motion'][0] for row in before)
        for j, row in enumerate(added):
            require(row['motion'][0] == maximum+1+j and row['motion'][1] == len(before)+j, 'Later unique IDs and exact order')
    return journals['assembled']


def native_lineage(record, job):
    """Exact dependency identity only; no geometry or material math approval."""
    native = record['native']
    has_departure = 'departure_canonical' in native
    has_later = 'later_canonical' in native
    require(not (has_departure and has_later), 'Unsupported combined program')
    native_fields = {'canonical', 'sha256', 'hatch_canonical', 'hatch_sha256', 'body_canonical', 'body_sha256',
                     'body', 'assembled', 'planned'}
    if has_departure:
        native_fields |= {'departure_canonical', 'departure_sha256', 'before', 'routed'}
    if has_later:
        native_fields |= {'later_canonical', 'later_sha256', 'later_prefixes'}
    require(set(native) == native_fields, 'Native evidence fields')
    for prefix in ['', 'hatch_', 'body_']:
        text, digest = native[prefix + 'canonical'], native[prefix + 'sha256']
        require(type(digest) is str and re.fullmatch('[a-f0-9]{64}', digest), 'Native digest')
        require(sha(text) == digest and canonical(parse(text)) == text, 'Native canonical identity')
    plan, hatch, body = (parse(native[key]) for key in ['canonical', 'hatch_canonical', 'body_canonical'])
    fields = {'schema', 'scope', 'job_fingerprint', 'attempt', 'hatch_lineage', 'assembled_journal', 'planned_journal', 'candidate_sha256'}
    if has_departure:
        fields.add('departure_lineage')
    if has_later:
        fields.add('later_lineage')
    require(set(plan) == fields, 'Native plan fields')
    require(set(hatch) == {'schema', 'scope', 'body_lineage', 'request', 'geometry'}, 'Native hatch fields')
    require(set(body) == {'schema', 'scope', 'job_fingerprint', 'attempt', 'source_sha256',
                          'slicing_input', 'request', 'body', 'body_material', 'body_journal'}, 'Native body fields')
    require(type(plan['schema']) is int and plan['schema'] == (3 if has_later else 2 if has_departure else 1) and
            plan['scope'] == ('owned_native_body_cap_later_linear_candidate_lineage_only' if has_later else
                              'owned_native_body_cap_departure_linear_candidate_lineage_only' if has_departure else
                              'owned_native_body_cap_linear_candidate_lineage_only'), 'Native plan version/scope')
    for document, scope in [(hatch, 'owned_native_affine_hatch_dependency_lineage_only'),
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
    for name in ['assembled', 'planned', 'body'] + (['before', 'routed'] if has_departure else []):
        journals[name] = decoded_journal(native[name], record['job_revision'])
    require(native['body']['sha256'] == body['body_journal'] and native['assembled']['sha256'] == plan['assembled_journal'] and
            native['planned']['sha256'] == plan['planned_journal'], 'Native ledger dependency')
    require(journals['body'][0]['source'] == body['body'].encode().hex(), 'Native body geometry journal binding')
    original_context, original = journals['assembled']
    if has_departure:
        original_context, original = departure_lineage(native, plan, journals)
    if has_later:
        original_context, original = later_lineage(native, plan, journals, record['job_revision'])
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
        has_departure = 'departure_canonical' in record['native']
        has_later = 'later_canonical' in record['native']
        manifest['schema'] = 4 if has_later else 3 if has_departure else 2
        manifest['scope'] = ('owned_native_body_cap_later_candidate_lineage_only' if has_later else
                             'owned_native_body_cap_departure_candidate_lineage_only' if has_departure else
                             'owned_native_body_cap_candidate_lineage_only')
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
        if name == 'material' and replay['rate_status'] != 'PASS':
            require(replay['material_status'] == 'UNKNOWN' and replay['material_reason'] == '' and
                    indexed['final_material']['status'] == 'UNKNOWN' and indexed['final_material']['execution'] == 'SKIPPED' and
                    indexed['final_material']['reason'] == 'Material replay skipped after final-rate refusal.' and
                    indexed['independent_replay']['reason'] == 'Complete declared journal replay skipped after final-rate refusal.', 'Exact skipped replay after rate refusal')
        else:
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
