#!/usr/bin/env python3
"""Independent exact dose/containment/accounting checks of the native cap witness."""
import argparse
import copy
import json
from fractions import Fraction
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def exact(value):
    require(type(value) in (int, float), 'Numeric witness')
    return Fraction(value)


def verify(document):
    require(document['schema'] == 1 and document['first_cap_contract'] in (4, 5), 'Complete fill contract')
    require(document['export'] == 'BLOCK', 'No export permission')
    require(document['exterior_parts'] == 0, 'Original native cap has no exterior domain')
    low, high = [[exact(v) for v in point] for point in document['domain']]
    amount = Fraction(0)
    require(len(document['packets']) == 161, 'Original actual native packets')
    for packet in document['packets']:
        a, b = [[exact(v) for v in packet[key]] for key in ('start', 'end')]
        x, y = a[1] == b[1], a[0] == b[0]
        require(x != y, 'Original axis packets')
        width = exact(packet['width'][1]) / 2
        bottom = min(a[2], b[2]) - max(map(exact, packet['gap']))
        require(low[2] <= bottom and max(a[2], b[2]) <= high[2], 'Whole vertical packet domain')
        for axis, transverse in ((0, not x), (1, x)):
            growth = width if transverse else Fraction(0)
            require(low[axis] <= min(a[axis], b[axis]) - growth and
                    max(a[axis], b[axis]) + growth <= high[axis], 'Whole XY packet domain')
        amount += exact(packet['volume'])
    ranges = {}
    for key in ('union', 'individual', 'repeated', 'target', 'covered', 'missing',
                'local_outside', 'outside_domain', 'outside_target'):
        bounds = tuple(map(exact, document[key + '_mm3']))
        require(0 <= bounds[0] <= bounds[1] and bounds[1] - bounds[0] <= Fraction(1, 1000), key + ' precision')
        ranges[key] = bounds
    require(ranges['individual'][0] <= amount <= ranges['individual'][1], 'Original exact packet dose')
    require(ranges['union'][0] + ranges['repeated'][0] <= amount <=
            ranges['union'][1] + ranges['repeated'][1], 'Union plus multiplicity equals dose')
    for whole, a, b in (('target', 'covered', 'missing'), ('union', 'covered', 'outside_target')):
        require(ranges[whole][0] <= ranges[a][1] + ranges[b][1] and
                ranges[a][0] + ranges[b][0] <= ranges[whole][1], whole + ' partition')
    require(ranges['outside_domain'] == (0, 0), 'No clipped-away native material')
    require(ranges['outside_target'] == ranges['local_outside'], 'Complete original spill')
    require(ranges['missing'][0] > 0 and ranges['repeated'][0] > 0, 'Original deficits and repetition retained')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    args = parser.parse_args()
    document = json.loads(args.input.read_text())
    verify(document)
    refused = 0
    for key, value in (('export', 'PASS'), ('exterior_parts', 1), ('missing_mm3', [0, 0]),
                       ('individual_mm3', [0, 0]), ('outside_domain_mm3', [.1, .1])):
        changed = copy.deepcopy(document)
        changed[key] = value
        try:
            verify(changed)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Changed accounting accepted: ' + key)
    print(json.dumps({'status': 'PASS', 'packets': len(document['packets']), 'mutation_refusals': refused,
                      'scope': 'EXACT_ORIGINAL_DOSE_AND_WHOLE_PACKET_DOMAIN_ACCOUNTING_ONLY', 'export': 'BLOCK'}))


if __name__ == '__main__':
    main()
