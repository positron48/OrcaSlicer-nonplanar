#!/usr/bin/env python3
"""Independent later request/prefix identity and actual final-byte replay only."""
import argparse
import copy
from pathlib import Path

from job_report_oracle import canonical, parse, require, verify as verify_report
from native_analysis_json_oracle import verify_document, verify_diagnostic
from native_worker_oracle import verify as verify_worker


def verify(record, document, diagnostic, initial_acceleration=40):
    verify_report(record)
    verify_document(document, record, initial_acceleration=initial_acceleration)
    verify_diagnostic(diagnostic, record)
    native = record['native']
    later = parse(native['later_canonical'])
    require(len(later['paths']) == 2 and [r[0] for r in later['requests']] == [1, 2], 'Two original alternating later passes')
    first = later['paths'][0][1]
    require(any(r['e_mm'] > 0 and r['start_mm'][2] != r['end_mm'][2]
                for r in diagnostic['replay'][first:]), 'Actual positive later extrusion with nonzero final decimal Z')
    require(parse(native['body_canonical'])['source_sha256'] ==
            '74c05263a47b85e9c87764306fee5d9c70caed4180de658393e5a034e71a6668', 'Separate original shallow STL bytes')
    return len(diagnostic['replay'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, required=True)
    args = parser.parse_args()
    read = lambda name: parse((args.fixtures / name).read_text())
    document = read('native-later-controller-request.json')
    record = read('native-later-controller-report.json')
    diagnostic = read('native-later-controller-diagnostic.json')
    fine = verify(record, document, diagnostic)
    default = read('native-later-default-report.json')
    reference = read('native-later-default-diagnostic.json')
    count = verify(default, document, reference)
    verify_worker(read('native-later-worker-input.json'), read('native-later-worker-diagnostic.json'),
                  reference, default, initial_acceleration=40)
    original = verify(read('native-later-original-policy-report.json'),
                      read('native-later-original-policy-request.json'),
                      read('native-later-original-policy-diagnostic.json'), initial_acceleration=100)
    refused = 0
    for mode in range(8):
        changed, transport, output = copy.deepcopy(record), copy.deepcopy(document), copy.deepcopy(diagnostic)
        if mode == 0:
            transport['later_paths'].reverse()
        if mode == 1:
            transport['later_paths'][0][1] += .000001
        if mode == 2:
            transport['schema'] = 1
        if mode == 3:
            changed['native']['later_prefixes'][1]['records'].pop()
        if mode == 4:
            lineage = parse(changed['native']['later_canonical'])
            lineage['paths'][1][1] += 1
            changed['native']['later_canonical'] = canonical(lineage)
        if mode == 5:
            changed['native']['later_prefixes'].reverse()
        if mode == 6:
            output['replay'][-1]['end_mm'][2] += .000001
        if mode == 7:
            changed['native'].pop('later_canonical')
        try:
            verify(changed, transport, output)
        except (ValueError, KeyError, IndexError, TypeError):
            refused += 1
        else:
            raise ValueError('Changed later request/prefix/movement accepted')
    print(canonical({'status': 'PASS', 'mutation_refusals': refused, 'fine_records': fine,
                     'default_records': count, 'original_policy_records': original,
                     'scope': 'OWNED_LATER_REQUEST_PREFIX_ORDER_AND_ACTUAL_FINAL_DECIMAL_BYTE_REPLAY_ONLY',
                     'job_status': 'UNKNOWN', 'export': 'BLOCK'}))


if __name__ == '__main__':
    main()
