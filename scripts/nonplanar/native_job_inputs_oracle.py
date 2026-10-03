#!/usr/bin/env python3
"""Independent typed-input identity vectors for the native B13 fixtures.

Exact declaration and dependency identity only, never physical qualification.
The expected values are explicit fixture inputs, not planner-produced hashes.
"""
import argparse
import copy
import hashlib
import json
import struct

from job_report_oracle import canonical, parse, require, verify as verify_report


def bits(value):
    return struct.pack('>d', value).hex()


def point(values):
    return [bits(v) for v in values]


def box(lo, hi):
    return [point(lo), point(hi)]


def sha(text):
    return hashlib.sha256(text.encode('utf-8')).hexdigest()


def expected_resources(initial_acceleration=100):
    domain = box([-1, -1, -.1], [40, 40, 10])
    head = [[i+1, i, box([-.05, -.05, .5], [.05, .05, .8]), False, False] for i in range(6)]
    motion = [1, 91, 1, 0, False, 0, 1, *domain]
    for values in [[200, 200, 5], [1000, 1000, 50], [200, 200, 5], [1000, 1000, 50]]:
        motion.extend(point(values))
    motion.extend(bits(v) for v in [1.75, 1, 40, 400, 5, 12, 2, 200])
    motion_hash = sha('nptop-linear-motion-policy-v1' + canonical(motion))
    serializer_hash = sha('nptop-linear-candidate-policy-v1' + canonical([1, 92, 1, bits(initial_acceleration), 6, 9, 6, 6, 3]))
    values = [
        ('native-toolhead-v1', {'head': head, 'schema': 1, 'tip': [point([0, 0, 0]), bits(.2), bits(.5)]}),
        ('native-scene-v1', {'coverage': [domain, box([-5, -5, -5], [45, 45, 20]), bits(30), True, bits(0)],
                             'identity': [1, 41, 7, 0, False], 'obstacles': [], 'schema': 1}),
        ('native-material-v1', {'body': {'material': [[17, *[bits(v) for v in [.01, .01, .01, .01, 0]]],
                                                       1, 2, 3, 7, 8, bits(20), bits(30), bits(100)],
                                           'plate_origin': point([0, 0, 0])},
                               'replay': [93, 1, *[bits(v) for v in [5e-9, .0001, 1e-9, .02, 0]]], 'schema': 1}),
        ('native-clearance-v1', {'clearance': [bits(v) for v in [.01, 0, 0, 0, 2e-6, 0, 0, 0, 0]], 'schema': 1}),
        ('native-motion-v1', {'motion_policy': motion_hash.encode().hex(), 'schema': 1}),
        ('native-serializer-v1', {'schema': 1, 'serializer_policy': serializer_hash.encode().hex()}),
    ]
    return [{'kind': i, 'name': name, 'bytes': canonical(value), 'sha256': sha(canonical(value))}
            for i, (name, value) in enumerate(values, 1)]


def verify_inputs(record, initial_acceleration=100):
    resources = record['native_inputs']
    require(resources == expected_resources(initial_acceleration), 'Exact native typed input vectors')
    job = parse(record['job_canonical'])
    roles = [r for r in job['resources'] if 1 <= r[0] <= 6]
    require(roles == [[r['kind'], r['name'].encode().hex(), r['sha256'].encode().hex(), len(r['bytes'].encode())]
                     for r in resources], 'Typed resources belong to the actual job')
    require(parse(resources[4]['bytes'])['motion_policy'] == record['motion_policy'].encode().hex(), 'Actual motion policy')
    require(parse(resources[5]['bytes'])['serializer_policy'] == record['serializer_policy'].encode().hex(), 'Actual serializer policy')
    native = record['native']
    body = parse(native['body_canonical'])['request']
    declaration = parse(resources[2]['bytes'])['body']
    require(body['material'] == declaration['material'][0]+declaration['material'][1:] and
            body['plate_origin'] == declaration['plate_origin'], 'Actual body reconstruction input')
    if 'departure_canonical' in native:
        departure = parse(native['departure_canonical'])
        scene = departure['scene']
        require(scene['head'] == parse(resources[0]['bytes'])['head'] and scene['tip'] == parse(resources[0]['bytes'])['tip'], 'Actual departure tool')
        require({k: scene[k] for k in ['coverage', 'identity', 'obstacles']} ==
                {k: parse(resources[1]['bytes'])[k] for k in ['coverage', 'identity', 'obstacles']}, 'Actual departure scene')
        require(departure['clearance'] == parse(resources[3]['bytes'])['clearance'], 'Actual departure clearance')


def mutations(record):
    refused = 0
    for role in range(6):
        changed = copy.deepcopy(record)
        changed['native_inputs'][role]['bytes'] += ' '
        changed['native_inputs'][role]['sha256'] = sha(changed['native_inputs'][role]['bytes'])
        try:
            verify_inputs(changed)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Changed typed resource accepted')
    for field in ['motion_policy', 'serializer_policy', 'job_canonical']:
        changed = copy.deepcopy(record)
        if field == 'job_canonical':
            job = parse(changed[field]);job['resources'][1][2] = '30'*64;changed[field] = canonical(job)
        else:
            changed[field] = '0'*64
        try:
            verify_inputs(changed)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Changed actual binding accepted')
    return refused


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True)
    args = parser.parse_args()
    with open(args.input) as stream:
        record = parse(stream.read())
    verify_report(record)
    verify_inputs(record)
    print(json.dumps({'status': 'PASS', 'resources': 6, 'mutation_refusals': mutations(record),
                      'scope': 'EXACT_TYPED_NATIVE_INPUT_AND_DEPENDENCY_IDENTITY_ONLY', 'export': 'BLOCK'}))


if __name__ == '__main__':
    main()
