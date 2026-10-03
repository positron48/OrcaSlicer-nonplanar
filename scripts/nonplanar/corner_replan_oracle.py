#!/usr/bin/env python3
"""Independent exact packet retention, finite-domain and accounting witness checks.

This checks declared simulation material; it does not certify arbitrary unions,
physical corner bonding, connectors, head/contact, a closed seam or export.
"""
import argparse
import copy
import json
from fractions import Fraction as Q
from pathlib import Path


def require(condition, message):
    if not condition:
        raise ValueError(message)


def interval(value):
    lo, hi = map(Q, value)
    require(lo <= hi, 'Ordered interval')
    return lo, hi


def contains(bounds, value):
    lo, hi = interval(bounds)
    require(lo <= value <= hi, 'Exact amount containment')


def difference(bounds, new, old):
    lo, hi = interval(bounds)
    a, b = interval(new)
    c, d = interval(old)
    require(lo <= b - c and a - d <= hi, 'Difference accounting')


def verify(document):
    require(document['schema'] == 1 and document['first_cap_contract'] == 5, 'Contract')
    require(document['export'] == 'BLOCK', 'No export')
    require(document['scope'] == 'PROSPECTIVE_CORNER_MATERIAL_ONLY_NOT_CLOSED_SEAM_OR_COMPLETE_CAP', 'Scope')
    axis = document['axis']
    require(axis in (0, 1), 'Axis')
    low = list(map(Q, document['roi'][:2]))
    high = list(map(Q, document['roi'][2:]))
    band = Q(document['band']) + Q(document['numeric'])
    gain, spill, repeated = map(Q, document['policy'])
    require(gain > 0 and spill >= 0 and repeated >= 0 and band >= 0, 'Explicit policy')
    before, after = document['before'], document['after']
    require(len(before['paths']) == len(after['paths']) == 5, 'Native path owners')
    require(sum(map(len, before['paths'])) == 173, 'Original native narrow packets')
    require(before['target'] == after['target'], 'Original target retained')
    sums = []
    for owner in (before, after):
        amount = Q(0)
        for path in owner['paths']:
            require(path, 'Nonempty paths')
            for index, packet in enumerate(path):
                a, b = [list(map(Q, packet[key])) for key in ('start', 'end')]
                x, y = a[1] == b[1], a[0] == b[0]
                require(x != y, 'Finite axis packet')
                if index:
                    require(path[index - 1]['end'] == packet['start'], 'Packet continuity')
                half = Q(packet['width'][1]) / 2
                for dim, transverse in ((0, not x), (1, x)):
                    growth = half if transverse else Q(0)
                    require(low[dim] + band <= min(a[dim], b[dim]) - growth and
                            max(a[dim], b[dim]) + growth <= high[dim] - band, 'Original boundary band')
                dose = Q(packet['volume'])
                require(dose > 0 and packet['kind'] == 1, 'Positive rounded dose')
                length = abs(b[0 if x else 1] - a[0 if x else 1])
                hmin, hmax = sorted(map(Q, packet['gap']))
                require(0 < hmin <= hmax, 'Positive gap')
                pi_lo = Q('3.14159265358979323846264338327950288419716939937510')
                pi_hi = Q('3.14159265358979323846264338327950288419716939937511')
                area = dose / length
                # w(h)=A/h+(1-pi/4)h decreases on the certified stadium domain.
                require(area > pi_hi * hmax * hmax / 4, 'Stadium domain')
                minimum = area / hmax + (1 - pi_hi / 4) * hmax
                maximum = area / hmin + (1 - pi_lo / 4) * hmin
                wlo, whi = interval(packet['width'])
                require(wlo <= minimum and maximum <= whi, 'Actual constant-flux width bounds')
                amount += dose
        contains(owner['individual'], amount)
        ranges = {key: interval(owner[key]) for key in ('individual', 'union', 'repeated', 'target', 'covered', 'missing', 'spill')}
        for lo, hi in ranges.values():
            require(0 <= lo <= hi and hi - lo <= Q(1, 1000), 'Original final precision')
        require(ranges['union'][0] + ranges['repeated'][0] <= amount <=
                ranges['union'][1] + ranges['repeated'][1], 'Union multiplicity accounting')
        for total, a, b in (('target', 'covered', 'missing'), ('union', 'covered', 'spill')):
            require(ranges[total][0] <= ranges[a][1] + ranges[b][1] and
                    ranges[a][0] + ranges[b][0] <= ranges[total][1], 'Target partition')
        require(ranges['missing'][0] > 0, 'Remaining deficit visible')
        sums.append(amount)
    changed = 0
    for index, (old, new) in enumerate(zip(before['paths'], after['paths'])):
        selected = index < 4 and (old[0]['start'][1] == old[0]['end'][1]) == (axis == 0)
        if not selected:
            require(old == new, 'Other owners retained exactly')
            continue
        changed += 1
        # The old middle, including every original dose and section, is intact.
        start = next((i for i in range(1, len(new)) if new[i] == old[0]), None)
        require(start is not None and start + len(old) < len(new), 'Both new finite ends')
        require(new[start:start + len(old)] == old, 'Exact original packet retention')
    require(changed == 2, 'Two opposite contour owners')
    contains(document['commanded_increase'], sums[1] - sums[0])
    require(sums[1] > sums[0], 'Actual positive added dose')
    difference(document['gain'], after['covered'], before['covered'])
    difference(document['missing_reduction'], before['missing'], after['missing'])
    difference(document['repeated_increase'], after['repeated'], before['repeated'])
    require(interval(document['gain'])[0] >= gain and interval(document['missing_reduction'])[0] >= gain, 'Qualified gain')
    require(interval(document['repeated_increase'])[1] <= repeated, 'Overlap budget')
    require(interval(after['spill'])[1] <= spill, 'Spill budget')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    args = parser.parse_args()
    document = json.loads(args.input.read_text())
    verify(document)
    mutations = []
    for name in ('dose', 'retention', 'width', 'band', 'target', 'repeat', 'gain', 'deficit', 'export'):
        changed = copy.deepcopy(document)
        if name == 'dose':
            changed['after']['individual'] = [0, 0]
        elif name == 'retention':
            changed['after']['paths'][4][0]['nominal_width'] += .01
        elif name == 'width':
            changed['after']['paths'][0][0]['width'] = [0, 0]
        elif name == 'band':
            changed['band'] += 1
        elif name == 'target':
            changed['after']['target'] = [0, 0]
        elif name == 'repeat':
            changed['policy'][2] = 0
        elif name == 'gain':
            changed['gain'] = [0, 0]
        elif name == 'deficit':
            changed['after']['missing'] = [0, 0]
        else:
            changed['export'] = 'PASS'
        try:
            verify(changed)
        except ValueError:
            mutations.append(name)
        else:
            raise ValueError('Mutation accepted: ' + name)
    print(json.dumps({'status': 'PASS', 'original_packets': sum(map(len, document['before']['paths'])),
                      'new_packets': sum(map(len, document['after']['paths'])), 'mutation_refusals': mutations,
                      'scope': 'EXACT_RETENTION_FINITE_DOMAIN_DOSE_AND_ACCOUNTING_ONLY', 'export': 'BLOCK'}))


if __name__ == '__main__':
    main()
