#!/usr/bin/env python3
"""Independent continuous gap extrema and dose integrals on actual stadium roofs.

This checker uses exact endpoint/stationary extrema, not the planner's curvature
estimate. Complete rational slabs bracket the actual fixed-width target volume.
Only perpendicular paths across constant whole-butt rows are supported here.
Individual prospective paths do not qualify a complete cap, route or export.
"""
import argparse
import copy
import json
from fractions import Fraction as Q
from pathlib import Path

from infill_replan_oracle import PI_LO, PI_HI, sqrt_bounds
from job_report_oracle import require
from native_hatch_precision_oracle import interval, native_rows, roof_bounds


def roof_at(row, n, core, upper):
    _, centre, top, radius, _ = row
    u = max(abs(n - centre) - core, Q(0))
    require(u <= radius, 'Constant stadium shoulder domain')
    return top - radius + sqrt_bounds(radius * radius - u * u)[int(upper)]


def residual(rows, low, high, affine):
    """Bounds of max(row roof) - affine over every point of the interval."""
    lower, upper = None, None
    slope = (affine(high) - affine(low)) / (high - low)
    root = sqrt_bounds(1 + slope * slope)
    for row in rows:
        _, centre, top, radius, core = row
        if max(abs(low - centre), abs(high - centre)) <= core[0] + radius:
            # One same covering owner supplies a lower concave chord. Combining
            # the best lower endpoint owners would bridge a real valley.
            floor = min(roof_at(row, n, core[0], False) - affine(n) for n in (low, high))
            lower = floor if lower is None else max(lower, floor)
        a, b = max(low, centre - core[1] - radius), min(high, centre + core[1] + radius)
        if a > b:
            continue
        ceiling = max(roof_at(row, n, core[1], True) - affine(n) for n in (a, b))
        # rho'(n)=slope gives signed shoulder u=-slope*r/sqrt(1+slope²).
        # At that stationary point the residual simplifies exactly to
        # top-r-affine(centre-sign(slope)*core)+r*sqrt(1+slope²).
        offset = centre - (core[1] if slope > 0 else -core[1] if slope < 0 else Q(0))
        stationary = sorted(offset - slope * radius / r for r in root)
        if stationary[0] <= b and a <= stationary[1]:
            ceiling = max(ceiling, top - radius - affine(offset) + radius * root[1])
        upper = ceiling if upper is None else max(upper, ceiling)
    require(lower is not None and upper is not None and lower <= upper, 'Same-owner whole packet roof enclosure')
    return lower, upper


def verify_case(document, integrate=True):
    rows = native_rows(document, 1)
    axis, normal = document['body_axis'], 1 - document['body_axis']
    roi = list(map(Q, document['footprint']))
    width = Q(document['nominal_width'])
    gap_limit, width_limit, total_limit = map(Q, document['limits'][:3])
    max_roof, max_packets, max_work = document['limits'][3:]
    require(gap_limit == Q(.0001) and width_limit == Q(.002) and total_limit == Q(.0001) and
            max_roof == 4096 and max_packets == 4096 and max_work == 200000, 'Original single-path ceilings')
    require(document['paths'], 'Nonempty owned prospective paths')
    total_pieces, total_segments, total_error = 0, 0, Q(0)
    results = []
    for path in document['paths']:
        gap_error, width_error, volume_error, numeric = map(Q, (path['gap_error'], path['width_error'], path['volume_error'], path['numeric']))
        require(0 <= gap_error <= gap_limit and 0 <= width_error <= width_limit and
                0 <= volume_error <= total_limit / len(document['paths']) and
                gap_error + width_error <= numeric <= Q(.05), 'Charged gap/width/volume/numeric errors')
        require(0 < path['segments'] <= max_roof and 0 < len(path['pieces']) <= max_packets and
                0 < path['evaluations'] <= max_work, 'Original roof/packet/work ceilings')
        total_pieces += len(path['pieces']);total_segments += path['segments'];total_error += volume_error
        deposited, target_lo, target_hi = Q(0), Q(0), Q(0)
        prior = None
        for piece in path['pieces']:
            a, b = [list(map(Q, piece[k])) for k in ('start', 'end')]
            require(len(a) == len(b) == 3 and a[axis] == b[axis] and a[normal] < b[normal] and
                    (prior is None or a == prior), 'Complete perpendicular packet continuity')
            prior = b
            half = (width + width_limit) / 2
            require(roi[axis] <= a[axis] - half < a[axis] + half <= roi[axis + 2] and
                    roi[normal] <= a[normal] < b[normal] <= roi[normal + 2], 'Whole finite footprint in original ROI')
            low, high = a[normal], b[normal]
            z = lambda n: a[2] + (b[2] - a[2]) * (n - low) / (high - low)
            h0, h1 = map(Q, piece['gap'])
            model = lambda n: z(n) - h0 - (h1 - h0) * (n - low) / (high - low)
            difference = residual(rows, low, high, model)
            require(-gap_error <= difference[0] <= difference[1] <= gap_error, 'Continuous highest-roof gap')
            floor = residual(rows, low, high, lambda n: Q(0))[0]
            require(floor >= Q(document['support_plane']), 'Complete packet roof excludes older rows')
            true = residual(rows, low, high, z)
            hmin, hmax = -true[1], -true[0]
            dose, length = Q(piece['volume']), high - low
            require(dose > 0 and 0 < hmin <= hmax < width, 'Positive actual-gap rounded section')
            area = dose / length
            require(area > PI_HI * hmax * hmax / 4, 'Actual rounded width domain')
            wmin = area / hmax + (1 - PI_HI / 4) * hmax
            wmax = area / hmin + (1 - PI_LO / 4) * hmin
            require(width - width_error <= wmin <= wmax <= width + width_error, 'Whole actual-gap width error')
            stored = interval(piece['width'])
            require(stored[0] <= area / max(h0, h1) + (1 - PI_HI / 4) * max(h0, h1) and
                    area / min(h0, h1) + (1 - PI_LO / 4) * min(h0, h1) <= stored[1], 'Actual model section law')
            deposited += dose
            if integrate:
                step = length / 64
                for i in range(64):
                    left, right = low + i * step, low + (i + 1) * step
                    roof = roof_bounds(rows, left, right)
                    require(roof[0] >= Q(document['support_plane']), 'Closed-slab whole roof')
                    heights = (min(z(left), z(right)) - roof[1], max(z(left), z(right)) - roof[0])
                    require(0 < heights[0] <= heights[1] < width, 'Actual ideal gap integration domain')
                    # w*h-c*h² is increasing in this admitted h<w domain.
                    target_lo += step * (width * heights[0] - (1 - PI_LO / 4) * heights[0] ** 2)
                    target_hi += step * (width * heights[1] - (1 - PI_HI / 4) * heights[1] ** 2)
        target, delivered = interval(path['target']), interval(path['delivered'])
        require(delivered[0] <= deposited <= delivered[1] and
                max(abs(delivered[0] - target[1]), abs(delivered[1] - target[0])) <= volume_error, 'Complete dose/target error enclosure')
        if integrate:
            require(target[0] <= target_lo <= target_hi <= target[1], 'Complete independent actual-gap volume enclosure')
        results.append({'pieces': len(path['pieces']), 'segments': path['segments'], 'work': path['evaluations'],
                        'target_mm3': [float(target_lo), float(target_hi)] if integrate else None})
    require(total_segments <= max_roof and total_pieces <= max_packets and total_error <= total_limit, 'Observed aggregate counts and errors')
    return {'body_axis': axis, 'rows': [r[0] for r in rows], 'paths': results,
            'scope': 'INDIVIDUAL_PROSPECTIVE_PATHS_NOT_SHARED_DEADLINE_COMPLETE_CAP_OR_NATIVE_JOB'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, required=True)
    args = parser.parse_args()
    analytical = json.loads((args.fixtures / 'analytical-roof-chord.json').read_text())
    native = json.loads((args.fixtures / 'native-roof-chord.json').read_text())
    require(analytical['schema'] == native['schema'] == 1 and analytical['export'] == native['export'] == 'BLOCK' and
            analytical['scope'] == 'ANALYTICAL_FINITE_FIRST_HATCH_ROOF_CHORDS_ONLY' and
            native['scope'] == 'INDIVIDUAL_FINITE_FIRST_HATCH_ROOF_CHORDS_ONLY', 'Bounded scope/version')
    require(len(analytical['cases']) == 4 and [r['body_axis'] for r in analytical['cases']] == [0, 1, 0, 1] and
            len(native['paths']) == 5 and native['cap_refusal'].startswith('MATERIAL_UNION_WORK_LIMIT paths='), 'Actual directions and retained complete-cap refusal')
    cases = analytical['cases'] + [native]
    results = [verify_case(c) for c in cases]
    mutations = [lambda c: c['paths'][0].update(gap_error=0),
                 lambda c: c['paths'][0].update(width_error=0),
                 lambda c: c.update(nominal_width=.1),
                 lambda c: c['journal'].update(sha256='0' * 64),
                 lambda c: c['paths'][0]['pieces'][0]['gap'].__setitem__(0, .5),
                 lambda c: c['paths'][0]['pieces'][0].update(volume=1.),
                 lambda c: c['paths'][0]['pieces'][0]['end'].__setitem__(0, 999.),
                 lambda c: c['paths'][0].update(evaluations=200001),
                 lambda c: c['paths'][0].update(target=[1., 1.1]),
                 lambda c: c['paths'][0]['pieces'].pop(1)]
    for mutate in mutations:
        altered = copy.deepcopy(analytical['cases'][2]);mutate(altered)
        try:
            verify_case(altered, False)
        except (AssertionError, ValueError, KeyError):
            continue
        raise AssertionError('Mutated roof chord accepted')
    # A fabricated flat chord through different best endpoint owners would
    # silently cover the valley. Its endpoints and local dose can look valid;
    # the independent continuous maximum-roof bound must reject the interior.
    altered = copy.deepcopy(analytical['cases'][2]);path = altered['paths'][0]
    a, b = list(path['pieces'][0]['start']), list(path['pieces'][-1]['end'])
    a[1], b[1] = .48, .52
    end_roof = roof_bounds(native_rows(altered, 1), Q(a[1]), Q(a[1]))
    height = float(Q(a[2]) - (end_roof[0] + end_roof[1]) / 2)
    dose = (b[1] - a[1]) * (altered['nominal_width'] * height - (1 - float(PI_LO) / 4) * height * height)
    path.update(pieces=[{'start': a, 'end': b, 'gap': [height, height], 'volume': dose, 'width': [.39999, .40001]}],
                target=[dose - 1e-12, dose + 1e-12], delivered=[dose - 1e-12, dose + 1e-12],
                volume_error=1e-10, numeric=.003, segments=1, evaluations=1)
    try:
        verify_case(altered, False)
    except ValueError as error:
        require(str(error) == 'Continuous highest-roof gap', 'Independent valley mutation target')
    else:
        raise AssertionError('Flat endpoint bridge over a valley accepted')
    print(json.dumps({'status': 'PASS', 'cases': results, 'mutation_refusals': len(mutations) + 1,
                      'scope': 'CONTINUOUS_CONSTANT_STADIUM_GAP_WIDTH_AND_INDIVIDUAL_DOSE_ONLY', 'export': 'BLOCK'}))


if __name__ == '__main__':
    main()
