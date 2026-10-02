#!/usr/bin/env python3
"""Final-byte rigid deposition geometry: actual chronology, not metadata contact approval."""
import argparse, copy, hashlib, json, math, subprocess
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--executable', type=Path, required=True)
p.add_argument('--fixture-dir', type=Path, required=True)
p.add_argument('--output-dir', type=Path, required=True)
a = p.parse_args()
exe, root = a.executable.resolve(), a.output_dir.resolve()
root.mkdir(parents=True, exist_ok=False)
rate = json.loads((a.fixture_dir / 'rate.json').read_text())
material = json.loads((a.fixture_dir / 'material.json').read_text())
rate['initial_position'] = [0, 0, .5]
scene = {
    'version': 1, 'profile_id': 51, 'revision': 1, 'synthetic': True,
    'operator_confirmed_claim': False,
    'tip': {'center': [0, 0, 0], 'opening_radius_mm': .2, 'outer_radius_mm': .5},
    'head': [{'id': i + 1, 'role': i, 'min': [-.05, -.05, .5],
              'max': [.05, .05, .8], 'moving': False,
              'all_configurations_enclosed': False} for i in range(6)],
    'obstacles': [], 'nozzle_domain': {'min': [-100] * 3, 'max': [100] * 3},
    'scene_domain': {'min': [-100] * 3, 'max': [100] * 3},
    'obstacle_inventory_complete': True, 'unmodelled_parts_min_local_z_mm': 5,
    'uncertainty_mm': 0, 'clearance_mm': [0] * 9,
}
cases = [
    ('complete-two-packets', 'PASS'), ('reverse', 'PASS'), ('varying-gap', 'PASS'),
    ('rotated', 'PASS'), ('moving-envelope', 'PASS'), ('current-annulus', 'FAIL'),
    ('current-head', 'FAIL'), ('late-packet', 'FAIL'), ('static-head', 'FAIL'),
    ('changed-e', 'FAIL'), ('partial-block', 'UNKNOWN'), ('skip-first', 'UNKNOWN'),
    ('mixed-block', 'UNKNOWN'), ('wrong-role', 'UNKNOWN'), ('forged-contact', 'UNKNOWN'),
    ('contact-id', 'UNKNOWN'), ('confirmation', 'UNKNOWN'), ('moving-incomplete', 'UNKNOWN'),
    ('duplicate-key', 'UNKNOWN'), ('unknown-version', 'UNKNOWN'), ('boolean-count', 'UNKNOWN'),
]
records = []
for name, status in cases:
    work = root / name
    work.mkdir()
    m, s = copy.deepcopy(material), copy.deepcopy(scene)
    previous, rows, lines = [0, 0, .5], [], ['G90', 'M83', 'M400', 'M204 S4']

    def append(end, e, gap=.2):
        command = 'G1' + ''.join(f' {axis}{value:.9f}' for axis, value in zip('XYZ', end))
        if e:
            command += f' E{e:.9f}'
        lines.extend([command + ' F30', 'M400'])
        rows.append({'event_id': len(rows) + 1, 'sequence_index': len(rows),
                     'kind': 'deposit' if e else 'travel', 'start': list(previous), 'end': list(end),
                     'expected_nominal_volume_mm3': e * math.pi * rate['filament_diameter'] ** 2 / 4 / rate['flow'],
                     'expected_filament_mm': 0,
                     'section': {'kind': 'rectangle', 'gap_begin_mm': .2, 'gap_end_mm': gap} if e else None})
        previous[:] = end

    def pose(t):
        return [(-1 if name == 'reverse' else 1) * (.6 if name == 'rotated' else 1) * t,
                .8 * t if name == 'rotated' else 0, .5 + t if name == 'varying-gap' else .5]

    if name == 'current-annulus':
        append([3, 4, .6], .2, .25)
        count = 1
    else:
        append(pose(.005), .00001, .21 if name == 'varying-gap' else .2)
        append([3, 4, .5] if name == 'late-packet' else pose(.01), .2 if name == 'late-packet' else .00001)
        count = 2
    # A later large deposit is not an already laid obstacle. It is separated
    # by Travel, which also terminates the selected maximal Deposit block.
    append([3, 4, .7], 0)
    append([6, 8, .7], .2)
    if name == 'current-head': s['head'][4]['min'][2], s['head'][4]['max'][2] = -.15, -.1
    if name == 'static-head': s['obstacles'] = [{'min': [-.01, -.01, 1.14], 'max': [.02, .01, 1.16]}]
    if name in ['moving-envelope', 'moving-incomplete']:
        s['head'][1]['moving'] = True
        s['head'][1]['all_configurations_enclosed'] = name == 'moving-envelope'
    if name == 'confirmation': s['operator_confirmed_claim'] = True
    if name == 'forged-contact': s['deposition_contact'] = {'allow': True}
    if name == 'contact-id': rows[0]['contact_model_id'] = 7
    q = {'version': 2 if name == 'unknown-version' else 1, 'first_record': 0, 'record_count': count, 'scene': s}
    if name == 'partial-block': q['record_count'] = 1
    if name == 'skip-first': q['first_record'], q['record_count'] = 1, 1
    if name == 'mixed-block': q['record_count'] = count + 1
    if name == 'wrong-role': q['first_record'], q['record_count'] = count, 1
    if name == 'boolean-count': q['record_count'] = True
    m['events'] = rows
    for filename, value in [('rate.json', rate), ('material.json', m), ('query.json', q)]:
        (work / filename).write_text(json.dumps(value))
    body = '\n'.join(lines) + '\n'
    if name == 'changed-e': body = body.replace('E0.000010000', 'E0.000020000', 1)
    (work / 'candidate.txt').write_text(body)
    if name == 'duplicate-key':
        f = work / 'query.json'
        f.write_text(f.read_text().replace('"opening_radius_mm": 0.2', '"opening_radius_mm": 0.2, "opening_radius_mm": 0.2'))
    files = [work / f for f in ['rate.json', 'material.json', 'query.json', 'candidate.txt']]
    before = {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
    cmd = [str(exe), '--linear-deposition-geometry-only', *map(str, files)]
    run = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
    (work / 'stdout.json').write_bytes(run.stdout)
    (work / 'stderr.txt').write_bytes(run.stderr)
    report = json.loads(run.stdout)
    assert run.returncode == {'PASS': 0, 'FAIL': 2, 'UNKNOWN': 3}[status] and report['component_status'] == status, (name, run.returncode, report)
    assert report['component'] == 'final_byte_deposition_geometry' and report['job_status'] == 'UNKNOWN' and report['export_allowed'] is False
    if status == 'PASS': assert report['leaves'] > 0 and report['prefix_completed_records'] == 0
    if name in ['current-annulus', 'current-head', 'late-packet', 'static-head']: assert report['witness'] is not None
    if name in ['current-annulus', 'current-head']: assert report['witness']['progress'][0] > 0
    if name == 'current-head': assert report['witness']['component'] == 5
    if name == 'late-packet': assert report['witness']['record'] == 1
    assert before == {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
    records.append({'case': name, 'command': cmd, 'exit_code': run.returncode, 'component_status': status, 'inputs_unchanged': True})
(root / 'manifest.json').write_text(json.dumps({'cases': records, 'status': 'PASS', 'scope': 'RIGID_DEPOSITION_COMPONENT_ONLY_CONTACT_AND_JOB_EXPORT_BLOCKED'}, indent=2) + '\n')
print(json.dumps({'cases': len(records), 'status': 'PASS', 'job_export': 'BLOCK'}))
