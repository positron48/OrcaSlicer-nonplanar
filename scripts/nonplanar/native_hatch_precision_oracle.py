#!/usr/bin/env python3
"""Independent rational roof integrals for constant native stadium rows.

Read the complete actual journal. Bound the maximum nominal roof over every
closed transverse slab, including shoulders and overlaps. This deliberately
supports only constant-height axis rows spanning the complete selected ROI.
No cap path, physical support/contact, whole job or export is qualified.
"""
import argparse
import copy
import json
from fractions import Fraction as Q
from pathlib import Path

from infill_replan_oracle import PI_LO, PI_HI, sqrt_bounds
from job_report_oracle import decoded_journal, decoded_number, require


def interval(value):
    require(type(value) is list and len(value) == 2, 'Volume interval fields')
    lo, hi = map(Q, value)
    require(0 < lo <= hi, 'Positive ordered volume interval')
    return lo, hi


def overlaps(a, b):
    require(a[0] <= b[1] and b[0] <= a[1], 'Independent volume enclosure overlap')


def enclosed(independent, produced):
    require(produced[0] <= independent[0] <= independent[1] <= produced[1], 'Complete independent volume enclosure')


def native_rows(document):
    _, rows = decoded_journal(document['journal'], document['revision'])
    footprint = list(map(Q, document['footprint']))
    axis, plane = document['body_axis'], Q(document['support_plane'])
    selected = []
    for row in rows:
        if row['bead'] is None:
            continue
        motion, bead = row['motion'], row['bead']
        a, b = ([Q(decoded_number(v)) for v in motion[i]] for i in (3, 4))
        # An older row's roof cannot exceed its nozzle Z. It is discarded only
        # after the selected rows independently cover every slab above plane.
        if max(a[2], b[2]) <= plane:
            continue
        require(bead[0] == 1 and a[2] == b[2] and bead[1] == bead[2], 'Constant native stadium domain')
        h = Q(decoded_number(bead[1]))
        dose = Q(decoded_number(motion[7][1]))
        require(motion[7][0] == 1 and h > 0 and dose > 0, 'Actual positive native dose/height')
        length = sqrt_bounds(sum((b[i] - a[i]) ** 2 for i in (0, 1)))
        require(length[0] > 0, 'Native projected length')
        width = (dose / length[1] / h + (1 - PI_HI / 4) * h,
                 dose / length[0] / h + (1 - PI_LO / 4) * h)
        stored = [Q(decoded_number(v)) for v in bead[3]]
        require(h < width[0] <= width[1] and stored[0] <= width[0] <= width[1] <= stored[1], 'Actual stadium section enclosure')
        if any(max(a[i], b[i]) + width[1] / 2 < footprint[i] or
               min(a[i], b[i]) - width[1] / 2 > footprint[i + 2] for i in (0, 1)):
            continue
        require(a[1 - axis] == b[1 - axis] and a[axis] != b[axis] and
                min(a[axis], b[axis]) < footprint[axis] < footprint[axis + 2] < max(a[axis], b[axis]),
                'Whole finite native butts span selected ROI')
        selected.append((motion[0], a[1 - axis], a[2], h / 2,
                         ((width[0] - h) / 2, (width[1] - h) / 2)))
    require(len(selected) >= 3, 'Actual shoulders and multiple native rows')
    return selected


def roof_bounds(rows, low, high):
    lower, upper = None, None
    for _, centre, top, radius, core in rows:
        near = max(low - centre, centre - high, Q(0))
        far = max(abs(low - centre), abs(high - centre))
        if near <= core[1] + radius:
            u = max(near - core[1], Q(0))
            ceiling = top - radius + sqrt_bounds(radius * radius - u * u)[1]
            upper = ceiling if upper is None else max(upper, ceiling)
        if far <= core[0] + radius:
            u = max(far - core[0], Q(0))
            floor = top - radius + sqrt_bounds(radius * radius - u * u)[0]
            lower = floor if lower is None else max(lower, floor)
    require(lower is not None and upper is not None and lower <= upper, 'Whole closed slab roof enclosure')
    return lower, upper


def integrate(document, rows):
    roi = list(map(Q, document['footprint']))
    axis, normal = document['body_axis'], 1 - document['body_axis']
    low, high = roi[normal], roi[normal + 2]
    strips = [list(map(Q, r['footprint'])) for r in document['strips']]
    # One complete partition serves parent and every strip. Critical shoulder
    # cuts and strip boundaries supplement uniform rational slabs; no samples
    # establish containment or replace a whole-slab bound.
    cuts = {low + (high - low) * Q(i, 16384) for i in range(16385)}
    cuts.update(v for r in strips for v in (r[normal], r[normal + 2]))
    for _, centre, _, radius, core in rows:
        cuts.update(v for width in core for v in (centre - width - radius, centre - width,
                                                  centre, centre + width, centre + width + radius) if low < v < high)
    cuts = sorted(cuts)
    integrals = []
    for a, b in zip(cuts, cuts[1:]):
        floor, ceiling = roof_bounds(rows, a, b)
        require(floor >= Q(document['support_plane']), 'Independent whole ROI roof excludes older rows')
        integrals.append(((b - a) * floor, (b - a) * ceiling))

    def volume(rectangle):
        left, right = cuts.index(rectangle[normal]), cuts.index(rectangle[normal + 2])
        roof = (sum(v[0] for v in integrals[left:right]), sum(v[1] for v in integrals[left:right]))
        primary = rectangle[axis + 2] - rectangle[axis]
        z00, z10, z01 = map(Q, document['surface'])
        gx, gy = (z10 - z00) / (roi[2] - roi[0]), (z01 - z00) / (roi[3] - roi[1])
        midpoint = z00 + gx * ((rectangle[0] + rectangle[2]) / 2 - roi[0]) + gy * ((rectangle[1] + rectangle[3]) / 2 - roi[1])
        target = (rectangle[2] - rectangle[0]) * (rectangle[3] - rectangle[1]) * midpoint
        result = (target - primary * roof[1], target - primary * roof[0])
        require(0 < result[0] <= result[1] and result[1] - result[0] < Q(document['consumer_precision']) / 10,
                'Independent rational roof precision')
        return result

    parent = volume(roi)
    enclosed(parent, interval(document['volume']))
    enclosed(parent, interval(document['coarse_volume']))
    for rectangle, strip in zip(strips, document['strips']):
        enclosed(volume(rectangle), interval(strip['volume']))
    return {'interval_mm3': [float(v) for v in parent], 'closed_slabs': len(integrals),
            'native_rows': [r[0] for r in rows], 'strips': len(strips)}


def verify(document, integrate_roof=True):
    require(document['schema'] == 1 and document['scope'] == 'ACTUAL_NATIVE_ROOF_STRIP_PRECISION_ONLY' and
            document['export'] == 'BLOCK', 'Bounded scope')
    require(document['body_axis'] in (0, 1) and document['hatch_axis'] in (0, 1) and
            type(document['transverse']) is bool and
            (document['body_axis'] != document['hatch_axis']) == document['transverse'], 'Actual native directions')
    roi = list(map(Q, document['footprint']))
    require(len(roi) == 4 and roi[0] < roi[2] and roi[1] < roi[3], 'Finite original ROI')
    producer, consumer, requested = map(Q, (document['producer_precision'], document['consumer_precision'], document['requested_precision']))
    require(0 < producer <= min(requested, consumer / 2), 'Original and downstream precision')
    bounded = interval(document['volume'])
    coarse = interval(document['coarse_volume'])
    require(bounded[1] - bounded[0] <= producer and coarse[1] - coarse[0] > consumer and
            coarse[1] - coarse[0] <= requested and document['coarse_refusal'] == 'INTEGRAL_STRIP_GLOBAL_PRECISION', 'Coarse certificate cannot refine')
    require(document['cap_refusal'] == ('FIRST_HATCH_ROOF_SEGMENT_LIMIT' if document['transverse'] else 'FIRST_HATCH_ROOF_DEPTH_LIMIT'), 'Complete cap remains refused')
    require(0 < document['cells'] <= document['max_cells'] == 8191 and
            0 < document['evaluations'] <= document['max_evaluations'] == 200000, 'Original producer resource ceilings')
    normal = 1 - document['hatch_axis']
    previous = roi[normal]
    total = [Q(0), Q(0)]
    require(document['strips'], 'Nonempty strip partition')
    for strip in document['strips']:
        r = list(map(Q, strip['footprint']))
        require(r[normal] == previous < r[normal + 2] <= roi[normal + 2] and
                r[1 - normal] == roi[1 - normal] and r[3 - normal] == roi[3 - normal], 'Complete disjoint strip partition')
        previous = r[normal + 2]
        amount = interval(strip['volume'])
        total = [total[i] + amount[i] for i in (0, 1)]
    require(previous == roi[normal + 2], 'Original strip end retained')
    stored = interval(document['strip_total'])
    require(stored[0] <= total[0] <= total[1] <= stored[1] and stored[1] - stored[0] <= consumer, 'Outward complete strip sum')
    overlaps(bounded, stored)
    rows = native_rows(document)
    return integrate(document, rows) if integrate_roof else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, required=True)
    args = parser.parse_args()
    documents = [json.loads((args.fixtures / f'native-hatch-precision-{name}.json').read_text()) for name in ('parallel', 'transverse')]
    require(documents[0]['journal'] == documents[1]['journal'], 'Same complete actual native body')
    results = [verify(d) for d in documents]
    mutations = [lambda d: d.update(export='ALLOW'),
                 lambda d: d.update(producer_precision=.01),
                 lambda d: d.update(consumer_precision=.0001),
                 lambda d: d.update(cap_refusal='PASS'),
                 lambda d: d.update(transverse=True),
                 lambda d: d.update(cells=8192),
                 lambda d: d['journal'].update(sha256='0' * 64),
                 lambda d: d['strips'][0]['footprint'].__setitem__(1, 0),
                 lambda d: d['strips'].pop(),
                 lambda d: d['strips'][0].update(volume=[1., 1.1]),
                 lambda d: d.update(strip_total=[1., 1.1])]
    for mutate in mutations:
        altered = copy.deepcopy(documents[0]);mutate(altered)
        try:
            verify(altered, False)
        except (AssertionError, ValueError, KeyError):
            continue
        raise AssertionError('Mutated roof/precision witness accepted')
    changed = copy.deepcopy(documents[0]);changed['surface'][0] += .1
    try:
        verify(changed)
    except (AssertionError, ValueError):
        pass
    else:
        raise AssertionError('Changed native target accepted')
    print(json.dumps({'status': 'PASS', 'cases': results, 'mutation_refusals': len(mutations) + 1,
                      'scope': 'CONSTANT_NATIVE_ROOF_COMPLETE_STRIP_VOLUME_ONLY', 'export': 'BLOCK'}))


if __name__ == '__main__':
    main()
