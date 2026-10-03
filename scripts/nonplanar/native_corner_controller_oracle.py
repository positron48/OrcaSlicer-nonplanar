#!/usr/bin/env python3
"""Independent captured corner recipe, retained material and final-byte replay."""
import argparse
import copy
from pathlib import Path

from job_report_oracle import canonical, parse, require, verify as verify_report
from native_analysis_json_oracle import verify_document, verify_diagnostic
from native_later_oracle import verify as verify_later
from native_worker_oracle import verify as verify_worker


def verify(record, document, diagnostic, initial_acceleration=100):
    verify_report(record)
    verify_document(document, record, initial_acceleration)
    verify_diagnostic(diagnostic, record)
    require(document['schema'] == 3 and parse(record['native']['canonical'])['schema'] == 4 and
            diagnostic['manifest']['schema'] == 5, 'Explicit corner schema chain')
    native = record['native']
    before, after = native['corner_before']['records'], native['corner_after']['records']
    deposits = lambda rows: sum(parse(row)['bead'] is not None for row in rows)
    require(deposits(after) > deposits(before), 'Actual added material reaches the final candidate')
    require(any(row['e_mm'] > 0 and row['start_mm'][2] != row['end_mm'][2]
                for row in diagnostic['replay']), 'Actual decimal sloped deposition')
    return len(diagnostic['replay'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, required=True)
    args = parser.parse_args()
    read = lambda name: parse((args.fixtures / ('native-corner-' + name + '.json')).read_text())
    record, document, diagnostic = read('controller-report'), read('controller-request'), read('controller-diagnostic')
    fine = verify(record, document, diagnostic)
    default, reference = read('default-report'), read('default-diagnostic')
    count = verify(default, document, reference)
    worker, child = read('worker-input'), read('worker-diagnostic')
    verify_worker(worker, child, reference, default)
    later, later_document, later_diagnostic = read('later-report'), read('later-request'), read('later-diagnostic')
    later_count = verify(later, later_document, later_diagnostic, 40)
    verify_later(later, later_document, later_diagnostic)
    refused = 0
    for mode in range(12):
        changed, transport, output = copy.deepcopy(record), copy.deepcopy(document), copy.deepcopy(diagnostic)
        if mode < 3:
            transport['corner_replan'][mode] += .000001
        if mode == 3:
            del transport['corner_replan']
        if mode == 4:
            transport['schema'] = 2
        if mode == 5:
            transport['later_paths'].append([1, 0, 0, 1, 1, 4.0])
        if mode == 6:
            changed['native']['corner_after']['records'].pop()
        if mode == 7:
            changed['native']['corner_before'] = copy.deepcopy(changed['native']['corner_after'])
        if mode == 8:
            changed['native']['corner_sha256'] = '0' * 64
        if mode == 9:
            del changed['native']['corner_canonical']
        if mode == 10:
            output['replay'][-1]['e_mm'] += .000001
        if mode == 11:
            output['export_allowed'] = True
        try:
            verify(changed, transport, output)
        except (ValueError, KeyError, IndexError, TypeError):
            refused += 1
        else:
            raise ValueError('Changed corner recipe/material/movement/export accepted')
    for field in ['corner_replan', 'movement']:
        source, actual = copy.deepcopy(worker), copy.deepcopy(child)
        if field == 'corner_replan':
            source['request']['corner_replan'][2] += .000001
        else:
            actual['replay'][-1]['end_mm'][2] += .000001
        try:
            verify_worker(source, actual, reference, default)
        except (ValueError, KeyError, IndexError, TypeError):
            refused += 1
        else:
            raise ValueError('Changed child corner request/output accepted')
    changed = copy.deepcopy(later)
    changed['native']['later_prefixes'][0]['records'].pop()
    try:
        verify(changed, later_document, later_diagnostic, 40)
    except (ValueError, KeyError, IndexError, TypeError):
        refused += 1
    else:
        raise ValueError('Changed repaired first prefix accepted')
    print(canonical({'status': 'PASS', 'mutation_refusals': refused, 'fine_records': fine,
                     'default_records': count, 'later_records': later_count,
                     'scope': 'CAPTURED_REPAIR_RECIPE_RETAINED_MATERIAL_CHILD_AND_FINAL_DECIMAL_BYTES_ONLY',
                     'job_status': 'UNKNOWN', 'export': 'BLOCK'}))


if __name__ == '__main__':
    main()
