#!/usr/bin/env python3
"""Independent analytical final-byte travel fixtures; component PASS never exports a job."""
import argparse, copy, hashlib, json, math, subprocess
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--executable', type=Path, required=True)
p.add_argument('--fixture-dir', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
a = p.parse_args()
exe = a.executable.resolve()
root = a.output_dir.resolve()
root.mkdir(parents=True, exist_ok=False)
rate = json.loads((a.fixture_dir / 'rate.json').read_text())
material = json.loads((a.fixture_dir / 'material.json').read_text())
rate['initial_position'] = [0, 0, .5]
material['policy']['max_coordinate_delta_mm'] = 2e-6
scene = {
    'version': 1, 'profile_id': 51, 'revision': 1, 'synthetic': True,
    'operator_confirmed_claim': False,
    'tip': {'center': [0, 0, 0], 'opening_radius_mm': .2, 'outer_radius_mm': .5},
    'head': [{'id': i + 1, 'role': i, 'min': [-.05, -.05, .5 + i * .1],
              'max': [.05, .05, .55 + i * .1], 'moving': False,
              'all_configurations_enclosed': False} for i in range(6)],
    'obstacles': [], 'nozzle_domain': {'min': [-100] * 3, 'max': [100] * 3},
    'scene_domain': {'min': [-100] * 3, 'max': [100] * 3},
    'obstacle_inventory_complete': True, 'unmodelled_parts_min_local_z_mm': 5,
    'uncertainty_mm': 0, 'clearance_mm': [0] * 9,
}
cases = [(name, status) for name, status in [
    ('future-not-present', 'PASS'), ('annulus-hole', 'PASS'), ('offset-head', 'PASS'),
    ('enclosed-moving-head', 'PASS'), ('rounded-nominal', 'PASS'),
    ('past-material', 'FAIL'), ('middle-annulus', 'FAIL'), ('middle-head', 'FAIL'),
    ('rounded-contact', 'FAIL'), ('changed-dose', 'FAIL'),
    ('incomplete-head', 'UNKNOWN'), ('duplicate-head-id', 'UNKNOWN'),
    ('missing-role', 'UNKNOWN'), ('moving-not-enclosed', 'UNKNOWN'),
    ('incomplete-inventory', 'UNKNOWN'), ('confirmed-claim', 'UNKNOWN'),
    ('head-scene-coverage', 'UNKNOWN'), ('nozzle-coverage', 'UNKNOWN'),
    ('omitted-height', 'UNKNOWN'), ('unknown-field', 'UNKNOWN'),
    ('duplicate-key', 'UNKNOWN'), ('wrong-version', 'UNKNOWN'),
    ('negative-count', 'UNKNOWN'), ('boolean-count', 'UNKNOWN'),
    ('partial-block', 'UNKNOWN'), ('skip-first-leg', 'UNKNOWN'),
    ('bad-radius', 'UNKNOWN'), ('nonfinite', 'UNKNOWN'), ('nested-axis', 'UNKNOWN'),
    ('margin-clear', 'PASS'), ('uncertainty-required', 'UNKNOWN'),
]] + [(f'clearance-{i}-required', 'UNKNOWN') for i in range(9)]
records = []
for name, status in cases:
    work = root / name
    work.mkdir()
    m, s = copy.deepcopy(material), copy.deepcopy(scene)
    previous = [0, 0, .5]
    rows, lines = [], ['G90', 'M83', 'M400', 'M204 S4']

    def append(end, e=0, rounded=False):
        actual = list(end)
        if rounded:
            actual[0] += .000001
        command = 'G1' + ''.join(f' {axis}{value:.9f}' for axis, value in zip('XYZ', actual))
        if e:
            command += f' E{e:.9f}'
        lines.extend([command + ' F30', 'M400'])
        rows.append({'event_id': len(rows) + 1, 'sequence_index': len(rows),
                     'kind': 'deposit' if e else 'travel', 'start': list(previous), 'end': list(end),
                     'expected_nominal_volume_mm3': e * math.pi * rate['filament_diameter'] ** 2 / 4 / rate['flow'],
                     'expected_filament_mm': 0,
                     'section': {'kind': 'rectangle', 'gap_begin_mm': .2, 'gap_end_mm': .2} if e else None})
        previous[:] = end

    if name == 'annulus-hole':
        append([.01, 0, .5], .00001)
        append([.01, 0, 1])
        first, count = 1, 1
    elif name == 'past-material':
        append([3, 4, .5], .2)
        append([0, 0, .5])
        first, count = 1, 1
    elif 'required' in name or name == 'margin-clear':
        append([3, 0, .5])
        first, count = 0, 1
        s['obstacles'] = [{'min': [1.49, .51, .499], 'max': [1.51, .52, .501]}]
        if name.startswith('clearance-'):
            s['clearance_mm'][int(name.split('-')[1])] = .05
        elif name == 'uncertainty-required':
            s['uncertainty_mm'] = .05
    else:
        append([3, 4, .5], rounded=name == 'rounded-contact')
        if name in ['partial-block', 'skip-first-leg']:
            append([4, 4, .5])
        append([6, 8, .5], .2)
        first, count = (1, 1) if name == 'skip-first-leg' else (0, 1)

    if name == 'middle-annulus': s['obstacles'] = [{'min': [1.99, 1.99, .49], 'max': [2.01, 2.01, .51]}]
    if name == 'middle-head': s['obstacles'] = [{'min': [1.49, 1.99, 1.14], 'max': [1.51, 2.01, 1.16]}]
    if name in ['rounded-nominal', 'rounded-contact']:
        s['obstacles'] = [{'min': [3.5000009, 3.9999999, .4999999], 'max': [3.5000011, 4.0000001, .5000001]}]
    if name == 'offset-head':
        s['tip']['center'][0] = .1
        for h in s['head']:
            h['min'][0] += .1
            h['max'][0] += .1
    if name in ['enclosed-moving-head', 'moving-not-enclosed']:
        s['head'][0]['moving'] = True
        s['head'][0]['all_configurations_enclosed'] = name == 'enclosed-moving-head'
    if name == 'incomplete-head': s['head'].pop()
    if name == 'duplicate-head-id': s['head'][1]['id'] = 1
    if name == 'missing-role': s['head'][-1]['role'] = 0
    if name == 'incomplete-inventory': s['obstacle_inventory_complete'] = False
    if name == 'confirmed-claim': s['operator_confirmed_claim'] = True
    if name == 'head-scene-coverage': s['scene_domain']['max'][0] = 3
    if name == 'nozzle-coverage': s['nozzle_domain']['max'][0] = 2
    if name == 'omitted-height':
        s['unmodelled_parts_min_local_z_mm'] = 0
        s['obstacles'] = [{'min': [20, 20, 1], 'max': [21, 21, 2]}]
    if name == 'unknown-field': s['safe'] = True
    if name == 'bad-radius': s['tip']['opening_radius_mm'] = .5
    if name == 'nested-axis': s['head'][0]['min'][0] = [[0]]
    q = {'version': 2 if name == 'wrong-version' else 1, 'first_record': first, 'record_count': count, 'scene': s}
    if name == 'negative-count': q['first_record'] = -1
    if name == 'boolean-count': q['record_count'] = True
    m['events'] = rows
    for filename, value in [('rate.json', rate), ('material.json', m), ('query.json', q)]:
        (work / filename).write_text(json.dumps(value))
    body = '\n'.join(lines) + '\n'
    if name == 'changed-dose': body = body.replace('E0.200000000', 'E0.300000000')
    (work / 'candidate.txt').write_text(body)
    qfile = work / 'query.json'
    if name == 'duplicate-key': qfile.write_text(qfile.read_text().replace('"opening_radius_mm": 0.2', '"opening_radius_mm": 0.2, "opening_radius_mm": 0.2'))
    if name == 'nonfinite': qfile.write_text(qfile.read_text().replace('"opening_radius_mm": 0.2', '"opening_radius_mm": 1e999'))
    files = [work / f for f in ['rate.json', 'material.json', 'query.json', 'candidate.txt']]
    before = {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
    cmd = [str(exe), '--linear-travel-geometry-only', *map(str, files)]
    run = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
    (work / 'stdout.json').write_bytes(run.stdout)
    (work / 'stderr.txt').write_bytes(run.stderr)
    report = json.loads(run.stdout)
    code = {'PASS': 0, 'FAIL': 2, 'UNKNOWN': 3}[status]
    assert run.returncode == code and report['component_status'] == status, (name, run.returncode, report)
    assert report['component'] == 'final_byte_travel_geometry' and report['job_status'] == 'UNKNOWN' and report['export_allowed'] is False
    if status == 'PASS': assert report['leaves'] > 0 and report['prefix_completed_records'] == first
    if name in ['past-material', 'middle-annulus', 'middle-head', 'rounded-contact']: assert report['witness'] is not None
    assert before == {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
    records.append({'case': name, 'command': cmd, 'exit_code': run.returncode, 'component_status': status, 'inputs_unchanged': True})
(root / 'manifest.json').write_text(json.dumps({'cases': records, 'status': 'PASS', 'scope': 'DECLARED_TRAVEL_BLOCK_ONLY_JOB_EXPORT_BLOCKED'}, indent=2) + '\n')
print(json.dumps({'cases': len(records), 'status': 'PASS', 'job_export': 'BLOCK'}))
