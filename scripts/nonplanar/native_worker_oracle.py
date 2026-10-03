#!/usr/bin/env python3
"""Independent worker snapshot framing and actual final-byte movement linkage."""
import argparse
import copy
import hashlib
from pathlib import Path
import struct

from job_report_oracle import canonical, parse, require, verify as verify_record
from native_analysis_json_oracle import verify_diagnostic, verify_document
from native_cli_gate import check_diagnostic


def sha(text):
    return hashlib.sha256(text.encode()).hexdigest()


def mesh_sha(mesh):
    data = b'nptop-native-mesh-v1\0' + struct.pack('>Q', len(mesh['vertices_f32_mm']))
    for v in mesh['vertices_f32_mm']:
        data += struct.pack('>fff', *v)
    data += struct.pack('>Q', len(mesh['faces']))
    for f in mesh['faces']:
        data += struct.pack('>III', *f)
    data += struct.pack('>Q', len(mesh['properties']))
    for kind, area in mesh['properties']:
        data += struct.pack('>Id', kind, area)
    return hashlib.sha256(data).hexdigest()


def verify(input, diagnostic, reference, record):
    require(set(input) == {'schema', 'host_job', 'software_sha256', 'input_sha256', 'source',
                           'meshes', 'request', 'request_sha256', 'files', 'plate_origin'}, 'Input registry')
    require(input['schema'] == 1 and len(input['host_job']) == 4 and all(type(v) is int and v > 0 for v in input['host_job'][:3]), 'Host ticket')
    require(input['input_sha256'] == sha(canonical(input['source'])), 'Exact canonical source hash')
    require(len(input['source']['objects']) == len(input['meshes']), 'Object count')
    for object, meshes in zip(input['source']['objects'], input['meshes']):
        require(len(object['volumes']) == len(meshes), 'Volume count')
        for volume, payload in zip(object['volumes'], meshes):
            require(mesh_sha(payload['mesh']).encode().hex() == volume['mesh_sha256'], 'Independent original mesh bits/order/property framing')
            require(len(payload['annotations']) == 4, 'Every annotation role')
            for data, hash in zip(payload['annotations'], volume['annotations']):
                require(sha(','.join(canonical(v) for v in data)).encode().hex() == hash, 'Actual annotation framing')
    body = parse(record['native']['body_canonical'])
    require(body['slicing_input'] == input['input_sha256'], 'Actual executed source includes matrices/config/annotations')
    require(input['request_sha256'] == record['analysis_request_sha256'], 'Exact owned request')
    verify_document(input['request'], record)
    verify_record(record)
    verify_diagnostic(reference, record)
    check_diagnostic(diagnostic, True)
    require(sha(canonical(diagnostic['report'])) == diagnostic['report_sha256'] and
            sha(canonical(diagnostic['manifest'])) == diagnostic['manifest_sha256'] and
            sha(diagnostic['job']['canonical']) == diagnostic['job']['fingerprint'], 'Child actual identities')
    job = parse(diagnostic['job']['canonical'])
    require(job['native_input'] == input['input_sha256'].encode().hex() and diagnostic['job']['id'] == input['host_job'][0], 'Child source/host ticket')
    require(diagnostic['request_sha256'] == input['request_sha256'] and
            diagnostic['replay'] == reference['replay'] and
            diagnostic['manifest']['candidate_sha256'] == record['candidate_sha256'].encode().hex(), 'All actual final decimal movements/byte ranges')
    require(diagnostic['report']['validation']['export_decision'] == 'BLOCK', 'No export credential')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, required=True)
    args = parser.parse_args()
    read = lambda name: parse((args.fixtures / name).read_text())
    input, diagnostic = read('native-worker-input.json'), read('native-worker-diagnostic.json')
    reference, record = read('native-worker-reference-diagnostic.json'), read('native-worker-reference-report.json')
    verify(input, diagnostic, reference, record)
    refused = 0
    for field in ['mesh', 'annotation', 'source', 'request', 'movement']:
        a, b = copy.deepcopy(input), copy.deepcopy(diagnostic)
        if field == 'mesh':
            a['meshes'][0][0]['mesh']['vertices_f32_mm'][0][0] += .125
        if field == 'annotation':
            a['meshes'][0][0]['annotations'][0][1].append(True)
        if field == 'source':
            a['source']['objects'][0]['instances'][0][0][0] = '3ff0000000000001'
        if field == 'request':
            a['request']['hatches'][0] += .125
        if field == 'movement':
            b['replay'][0]['e_mm'] += .125
        try:
            verify(a, b, reference, record)
        except (ValueError, KeyError, IndexError, TypeError):
            refused += 1
        else:
            raise ValueError('Changed worker input/output accepted')
    refusal = read('native-worker-units-refusal.json')
    require(refusal['completed'] is False and refusal['export_allowed'] is False and not refusal['replay'] and refusal['report'] is None, 'No partial movements after refusal')
    print(canonical({'status': 'PASS', 'mutation_refusals': refused, 'replay_records': len(diagnostic['replay']),
                     'scope': 'OWNED_WORKER_SOURCE_BINARY_FRAMING_AND_ACTUAL_DECIMAL_MOVEMENTS_ONLY', 'export': 'BLOCK'}))


if __name__ == '__main__':
    main()
