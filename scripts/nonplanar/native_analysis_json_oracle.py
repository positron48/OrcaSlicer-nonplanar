#!/usr/bin/env python3
"""Independent editing-transport binary vectors and actual replay diagnostic linkage."""
import argparse
import copy
import hashlib
from pathlib import Path
import struct

from job_report_oracle import canonical, parse, require
from native_cli_gate import check_diagnostic
from native_controller_oracle import verify_request
from native_job_inputs_oracle import bits, expected_resources, point, verify_inputs


def sha(text):
    return hashlib.sha256(text.encode()).hexdigest()


def verify_document(document, record, initial_acceleration=100):
    verify_inputs(record, initial_acceleration)
    verify_request(record)
    resources = expected_resources(initial_acceleration)
    source = parse(record['analysis_request_canonical'])
    require(set(document) == {'schema', 'millimeters_declared', 'reservation', 'passes', 'hatches', 'contour',
                              'fill_region_mm', 'body', 'replay', 'scene', 'motion', 'serializer'} |
            ({'later_paths'} if source['schema'] >= 2 else set()) |
            ({'corner_replan'} if source['schema'] == 3 else set()), 'Exact transport registry')
    require(type(document['schema']) is int and document['schema'] == source['schema'] and
            document['millimeters_declared'] == source['millimeters_declared'], 'Version/units')
    if source['schema'] >= 2:
        require(type(document['later_paths']) is list and (source['schema'] == 3 or len(document['later_paths']) > 0) and len(document['later_paths']) <= 4096 and
                [[row[0], *[bits(n) for n in row[1:]]] for row in document['later_paths']] == source['later_paths'], 'Exact ordered later paths')
    else:
        require('later_paths' not in document, 'Legacy transport has no later paths')
    if source['schema'] == 3:
        require(type(document['corner_replan']) is list and len(document['corner_replan']) == 3 and
                all(type(n) in (int, float) for n in document['corner_replan']) and
                [bits(n) for n in document['corner_replan']] == source['corner_replan'], 'Exact optional corner policy')
    else:
        require('corner_replan' not in document, 'Legacy transport has no corner repair')
    mesh = document['reservation']
    framed = b'nptop-native-mesh-v1\0' + struct.pack('>Q', len(mesh['vertices_f32_mm']))
    for v in mesh['vertices_f32_mm']:
        require(len(v) == 3 and all(struct.unpack('>f', struct.pack('>f', n))[0] == n for n in v), 'Exact float32 vertex')
        framed += struct.pack('>fff', *v)
    framed += struct.pack('>Q', len(mesh['faces']))
    for f in mesh['faces']:
        framed += struct.pack('>III', *f)
    framed += struct.pack('>Q', len(mesh['properties']))
    for kind, area in mesh['properties']:
        framed += struct.pack('>Id', kind, area)
    require(hashlib.sha256(framed).hexdigest().encode().hex() == source['reservation'], 'Independent mesh framing')
    p = document['passes']
    require([bits(n) for n in p['footprint_mm']] == source['footprint'] and p['patch_index'] == source['patch'] and
            bits(p['support_plane_z_mm']) == source['support_plane'], 'Actual pass selection')
    require([p['policy'][0], *[bits(n) for n in p['policy'][1:]]] == source['passes'], 'Every pass tolerance')
    require([*[bits(n) for n in document['hatches'][:3]], document['hatches'][3]] == source['hatches'], 'Hatch policy')
    co = document['contour']
    require([bits(co[0]), co[1], co[2], bits(co[3])] == source['contour'] and
            [point(v) for v in document['fill_region_mm']] == source['fill_region'], 'Contour/fill choices')
    b, replay = document['body'], document['replay']
    body = {'plate_origin': point(b['plate_origin_mm']), 'material': [[b['model'][0], *[bits(n) for n in b['model'][1:]]],
             *b['material_ids'], *b['reference_ids'], *[bits(n) for n in b['rates']]]}
    require({'body': body, 'replay': [*replay['ids'], *[bits(n) for n in replay['tolerances']]], 'schema': 1} ==
            parse(resources[2]['bytes']), 'Exact body/replay declaration')
    s = document['scene']
    box = lambda b: [point(b['min']), point(b['max'])]
    require({'head': [[h['id'], h['role'], box(h), h['moving'], h['all_configurations_enclosed']] for h in s['head']],
             'schema': 1, 'tip': [point(s['tip']['center']), bits(s['tip']['opening_radius_mm']), bits(s['tip']['outer_radius_mm'])]} ==
            parse(resources[0]['bytes']), 'Complete head/tip')
    require({'coverage': [box(s['nozzle_domain']), box(s['scene_domain']), bits(s['unmodelled_parts_min_local_z_mm']), s['obstacle_inventory_complete'], bits(s['uncertainty_mm'])],
             'identity': [s['version'], s['profile_id'], s['revision'], 0 if s['synthetic'] else 1, s['operator_confirmed_claim']],
             'obstacles': [box(v) for v in s['obstacles']], 'schema': 1} == parse(resources[1]['bytes']), 'Complete scene')
    require({'clearance': [bits(n) for n in s['clearance_mm']], 'schema': 1} == parse(resources[3]['bytes']), 'Every clearance budget')
    m = document['motion']
    vector = [*m['ids'], m['origin'], m['operator_confirmed_claim'], m['model'], m['kinematics'], *[point(v) for v in m['commanded_domain_mm']]]
    for key in ['axis_speed_mm_s', 'axis_acceleration_mm_s2', 'drive_speed_mm_s', 'drive_acceleration_mm_s2', 'filament', 'limits']:
        vector.extend(bits(n) for n in m[key])
    require(sha('nptop-linear-motion-policy-v1'+canonical(vector)).encode().hex() == parse(resources[4]['bytes'])['motion_policy'], 'Every motion field')
    v = document['serializer']
    require(sha('nptop-linear-candidate-policy-v1'+canonical([1, *v['ids'], bits(v['initial_acceleration_mm_s2']), *v['digits']])).encode().hex() ==
            parse(resources[5]['bytes'])['serializer_policy'], 'Every serializer field')


def verify_diagnostic(diagnostic, record):
    check_diagnostic(diagnostic, True)
    require(diagnostic['report'] == parse(record['canonical']) and diagnostic['report_sha256'] == record['sha256'] and
            diagnostic['job']['canonical'] == record['job_canonical'] and diagnostic['job']['fingerprint'] == record['job_fingerprint'] and
            diagnostic['request_sha256'] == record['analysis_request_sha256'], 'Actual protected identities')
    position = record['initial_position']
    moves, ranges = [], []
    offset = 0
    for line in record['candidate_bytes'].splitlines(keepends=True):
        end = offset + len(line.encode())
        if line.startswith('G1 '):
            words = {w[0]: float(w[1:]) for w in line.split()[1:]}
            target = [words[k] for k in 'XYZ'] if 'X' in words else position
            moves.append((position, target, words.get('E', 0), words['F']))
            ranges.append([offset, end])
            position = target
        elif line.startswith('G4 '):
            moves.append((position, position, 0, 0))
            ranges.append([offset, end])
        offset = end
    require(len(moves) == len(diagnostic['replay']), 'Independent final-byte movement count')
    for row, (start, end, e, feed), byte_range in zip(diagnostic['replay'], moves, ranges):
        require(row['start_mm'] == start and row['end_mm'] == end and row['e_mm'] == e and row['feed_mm_min'] == feed,
                'Independent final decimal byte replay')
        require(row['candidate_byte_range'][0] <= byte_range[0] and byte_range[1] <= row['candidate_byte_range'][1], 'Actual movement range')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, required=True)
    args = parser.parse_args()
    document = parse((args.fixtures/'native-controller-request.json').read_text())
    record = parse((args.fixtures/'native-controller-report.json').read_text())
    diagnostic = parse((args.fixtures/'native-controller-diagnostic.json').read_text())
    verify_document(document, record)
    verify_diagnostic(diagnostic, record)
    default_record = parse((args.fixtures/'native-cli-reference-report.json').read_text())
    default_diagnostic = parse((args.fixtures/'native-cli-reference-diagnostic.json').read_text())
    verify_document(document, default_record)
    verify_diagnostic(default_diagnostic, default_record)
    refused = 0
    for key in ['body', 'scene', 'motion', 'serializer', 'replay', 'passes', 'hatches', 'contour', 'fill_region_mm', 'reservation']:
        changed = copy.deepcopy(document)
        changed[key] = {} if isinstance(changed[key], dict) else []
        try:
            verify_document(changed, record)
        except (ValueError, KeyError, IndexError, TypeError):
            refused += 1
        else:
            raise ValueError('Changed transport input accepted')
    print(canonical({'status': 'PASS', 'mutation_refusals': refused, 'replay_records': len(diagnostic['replay']),
                     'default_replay_records': len(default_diagnostic['replay']),
                     'scope': 'TRANSPORT_BINARY_VECTORS_ACTUAL_PROTECTED_DIAGNOSTIC_AND_FINAL_BYTE_MOVEMENTS_ONLY', 'export': 'BLOCK'}))


if __name__ == '__main__':
    main()
