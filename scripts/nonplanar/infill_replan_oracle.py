#!/usr/bin/env python3
"""Independent retained packet, pitch, dose and flat stadium-union checks.

Sloped unions are accounting witnesses only. No contact, seam, native-job,
physical qualification or export is inferred from these prospective fixtures.
"""
import argparse
import copy
import json
from fractions import Fraction as Q
from math import isqrt
from pathlib import Path

from corner_replan_oracle import contains, difference, interval, require

PI_LO = Q('3.14159265358979323846264338327950288419716939937510')
PI_HI = Q('3.14159265358979323846264338327950288419716939937511')


def sqrt_bounds(value):
    require(value >= 0, 'Nonnegative squared radius')
    scale = 10 ** 20
    root = isqrt(value.numerator * scale * scale // value.denominator)
    lo, hi = Q(root, scale), Q(root + 1, scale)
    require(lo * lo <= value <= hi * hi, 'Exact square root bounds')
    return lo, hi


def union_area(rectangles):
    cuts = sorted({r[i] for r in rectangles for i in (0, 2)})
    area = Q(0)
    for a, b in zip(cuts, cuts[1:]):
        spans = sorted((r[1], r[3]) for r in rectangles if r[0] <= a and b <= r[2])
        if not spans:
            continue
        lo, hi = spans[0]
        length = Q(0)
        for start, end in spans[1:]:
            if start > hi:
                length += hi - lo
                lo, hi = start, end
            else:
                hi = max(hi, end)
        area += (b - a) * (length + hi - lo)
    return area


def flat_union(paths):
    packets = [p for path in paths for p in path]
    height = Q(packets[0]['gap'][0])
    top = Q(packets[0]['start'][2])
    for p in packets:
        require(p['gap'][0] == p['gap'][1] and Q(p['gap'][0]) == height and
                Q(p['start'][2]) == Q(p['end'][2]) == top, 'Flat common stadium height')

    def area(z, outer):
        radial = sqrt_bounds(height * height / 4 - z * z)[int(outer)]
        rectangles = []
        for p in packets:
            a, b = [list(map(Q, p[key])) for key in ('start', 'end')]
            axis = 0 if a[1] == b[1] else 1
            length = abs(b[axis] - a[axis])
            pi = PI_LO if outer else PI_HI
            width = Q(p['volume']) / length / height + (1 - pi / 4) * height
            half = (width - height) / 2 + radial
            require(half >= 0, 'Positive flat core')
            rectangle = [min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1])]
            rectangle[1 - axis] -= half
            rectangle[3 - axis] += half
            rectangles.append(rectangle)
        return union_area(rectangles)

    # All cross sections decrease together on this half; exact rational rectangle
    # sweeps at the slab ends bracket the integral without planner geometry.
    slabs = 2048
    step = height / 2 / slabs
    lower = upper = Q(0)
    for i in range(slabs):
        lower += 2 * step * area((i + 1) * step, False)
        upper += 2 * step * area(i * step, True)
    require(upper - lower < Q(1, 10000), 'Independent flat union precision')
    return lower, upper


def verify(document, integrate=True):
    require(document['schema'] == 1 and document['first_cap_contract'] == 6, 'Contract')
    require(document['export'] == 'BLOCK' and document['scope'] ==
            'PROSPECTIVE_RETAINED_INFILL_ONLY_NOT_CLOSED_SEAM_COMPLETE_CAP_OR_NATIVE_JOB', 'Scope')
    require([(r['axis'], r['sloped']) for r in document['cases']] ==
            [(0, False), (0, True), (1, False), (1, True)], 'Both axes and actual slopes')
    packets = 0
    for case in document['cases']:
        axis = case['axis']
        pitch, gain, spill, repeat = map(Q, case['policy'])
        require(pitch > 0 and gain > 0 and spill >= 0 and repeat >= 0, 'Explicit policy')
        before, after = case['before'], case['after']
        require(len(after['paths']) > len(before['paths']) >= 5, 'Additional prospective owners')
        require(after['paths'][:len(before['paths'])] == before['paths'], 'Exact retained packets and doses')
        require(after['target'] == before['target'], 'Original target')
        extent = [before['paths'][4][0]['start'][axis], before['paths'][4][-1]['end'][axis]]
        sums = []
        for owner in (before, after):
            amount = Q(0)
            centres = []
            for index, path in enumerate(owner['paths']):
                require(path, 'Nonempty path')
                if path[0]['start'][1 - axis] == path[-1]['end'][1 - axis]:
                    centres.append(Q(path[0]['start'][1 - axis]))
                if index >= len(before['paths']):
                    require([path[0]['start'][axis], path[-1]['end'][axis]] == extent, 'Original longitudinal extent')
                    require(path[0]['start'][1 - axis] == path[-1]['end'][1 - axis], 'Original parallel axis')
                for i, p in enumerate(path):
                    a, b = [list(map(Q, p[key])) for key in ('start', 'end')]
                    x, y = a[1] == b[1], a[0] == b[0]
                    require(x != y and p['kind'] == 1 and p['nominal_width'] == .45, 'Fixed-width axis stadium')
                    if i:
                        require(path[i - 1]['end'] == p['start'], 'Packet continuity')
                    length = abs(b[0 if x else 1] - a[0 if x else 1])
                    dose = Q(p['volume'])
                    require(dose > 0, 'Positive actual dose')
                    hmin, hmax = sorted(map(Q, p['gap']))
                    area = dose / length
                    require(0 < hmin <= hmax and area > PI_HI * hmax * hmax / 4, 'Stadium domain')
                    lo, hi = interval(p['width'])
                    require(lo <= area / hmax + (1 - PI_HI / 4) * hmax and
                            area / hmin + (1 - PI_LO / 4) * hmin <= hi, 'Actual dose/width law')
                    amount += dose
                    packets += 1
            contains(owner['individual'], amount)
            ranges = {k: interval(owner[k]) for k in ('individual', 'union', 'repeated', 'target', 'covered', 'missing', 'spill')}
            for lo, hi in ranges.values():
                require(0 <= lo <= hi and hi - lo <= Q(1, 1000), 'Original final precision')
            require(ranges['union'][0] + ranges['repeated'][0] <= amount <=
                    ranges['union'][1] + ranges['repeated'][1], 'Dose union multiplicity')
            for total, a, b in (('target', 'covered', 'missing'), ('union', 'covered', 'spill')):
                require(ranges[total][0] <= ranges[a][1] + ranges[b][1] and
                        ranges[a][0] + ranges[b][0] <= ranges[total][1], 'Whole target partition')
            if owner is after:
                centres = sorted(set(centres))
                require(all(b - a <= pitch for a, b in zip(centres, centres[1:])), 'Actual stored pitch')
            if not case['sloped'] and integrate:
                lo, hi = flat_union(owner['paths'])
                require(ranges['union'][0] <= hi and lo <= ranges['union'][1], 'Independent flat union')
            sums.append(amount)
        contains(case['commanded_increase'], sums[1] - sums[0])
        require(sums[1] > sums[0], 'Additional dose')
        difference(case['gain'], after['covered'], before['covered'])
        difference(case['missing_reduction'], before['missing'], after['missing'])
        difference(case['repeated_increase'], after['repeated'], before['repeated'])
        require(interval(case['gain'])[0] >= gain and interval(case['missing_reduction'])[0] >= gain, 'Actual gain')
        require(interval(case['repeated_increase'])[1] <= repeat and interval(after['spill'])[1] <= spill, 'Overlap and spill ceilings')
    return packets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    args = parser.parse_args()
    document = json.loads(args.input.read_text())
    packets = verify(document)
    for name in ('dose', 'retention', 'width', 'pitch', 'target', 'repeat', 'gain', 'union', 'axis', 'export'):
        changed = copy.deepcopy(document)
        case = changed['cases'][0]
        if name == 'dose':
            case['after']['paths'][-1][0]['volume'] *= 2
        elif name == 'retention':
            case['after']['paths'][0][0]['start'][0] += .01
        elif name == 'width':
            case['after']['paths'][-1][0]['width'] = [0, 0]
        elif name == 'pitch':
            case['policy'][0] = .001
        elif name == 'target':
            case['after']['target'] = [1, 1]
        elif name == 'repeat':
            case['policy'][3] = 0
        elif name == 'gain':
            case['gain'] = [0, 0]
        elif name == 'union':
            case['before']['union'] = [0, 0]
        elif name == 'axis':
            case['axis'] = 1
        else:
            changed['export'] = 'PASS'
        try:
            verify(changed, integrate=False)
        except ValueError:
            continue
        raise ValueError('Mutation accepted: ' + name)
    print(json.dumps({'status': 'PASS', 'cases': 4, 'packet_observations': packets, 'flat_union_integrals': 4,
                      'mutation_refusals': 10, 'sloped_union': 'ACCOUNTING_ONLY', 'native_binding': 'NOT_IMPLEMENTED', 'export': 'BLOCK'}))


if __name__ == '__main__':
    main()
