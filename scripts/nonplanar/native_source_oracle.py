#!/usr/bin/env python3
"""Independent source-only worker framing and exact final-byte replay linkage."""
import argparse
import copy
from pathlib import Path

from job_report_oracle import canonical, parse, require
from native_worker_oracle import sha, verify_result, verify_source


def verify(input, owner, diagnostic, reference, record):
    require(set(input) == {'schema', 'host_view', 'software_sha256', 'input_sha256', 'source',
                           'meshes', 'request', 'request_sha256', 'files', 'plate_origin'}, 'Source registry')
    require(input['schema'] == 2 and len(input['host_view']) == 3 and
            all(type(v) is int and v > 0 for v in input['host_view'][:2]) and
            input['host_view'] == owner['host_view'], 'Actual separately recorded display owner')
    files = dict(input['files'])
    require(len(files) == len(input['files']), 'Unique source files')
    editing = bytes.fromhex(files['native-analysis-json-v1']).decode()
    require(sha(editing) == owner['editing_sha256'] and
            canonical(parse(editing)) == canonical(input['request']), 'Exact editing bytes and parsed request')
    verify_source(input, record)
    verify_result(input, diagnostic, reference, record, input['host_view'][0])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, required=True)
    args = parser.parse_args()
    read = lambda name: parse((args.fixtures / name).read_text())
    input, owner = read('native-source-input.json'), read('native-source-owner.json')
    diagnostic = read('native-source-diagnostic.json')
    reference, record = read('native-source-reference-diagnostic.json'), read('native-source-reference-report.json')
    verify(input, owner, diagnostic, reference, record)
    refused = 0
    for field in ['mesh', 'annotation', 'source', 'request', 'owner', 'editing', 'movement']:
        a, b = copy.deepcopy(input), copy.deepcopy(diagnostic)
        if field == 'mesh':
            a['meshes'][0][0]['mesh']['vertices_f32_mm'][0][0] += .125
        elif field == 'annotation':
            a['meshes'][0][0]['annotations'][0][1].append(True)
        elif field == 'source':
            a['source']['objects'][0]['instances'][0][0][0] = '3ff0000000000001'
        elif field == 'request':
            a['request']['hatches'][0] += .125
        elif field == 'owner':
            a['host_view'][1] += 1
        elif field == 'editing':
            a['files'] = [[n, v + '20' if n == 'native-analysis-json-v1' else v] for n, v in a['files']]
        else:
            b['replay'][0]['e_mm'] += .125
        try:
            verify(a, owner, b, reference, record)
        except (ValueError, KeyError, IndexError, TypeError):
            refused += 1
        else:
            raise ValueError('Changed source input/output accepted')
    for name in ['hook', 'policy', 'units']:
        refusal = read('native-source-' + name + '-refusal.json')
        require(refusal['completed'] is False and refusal['export_allowed'] is False and
                not refusal['replay'] and refusal['report'] is None, 'No partial policy/units refusal')
    print(canonical({'status': 'PASS', 'mutation_refusals': refused, 'replay_records': len(diagnostic['replay']),
                     'scope': 'SOURCE_ONLY_WORKER_PUBLISHED_FRAMING_EDITING_BYTES_AND_ACTUAL_FINAL_DECIMALS_ONLY',
                     'private_owner': 'RECORDED_BINDING_NOT_INDEPENDENT_PRIVATE_IDENTITY_OR_EXPORT_PROOF', 'export': 'BLOCK'}))


if __name__ == '__main__':
    main()
