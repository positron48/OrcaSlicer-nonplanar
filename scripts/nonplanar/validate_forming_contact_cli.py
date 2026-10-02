#!/usr/bin/env python3
"""Bounded recent forming-run working-face contact; no full-job or physical approval."""
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
contact = {
    'version': 1, 'model_id': 101, 'revision': 1, 'profile_id': 51,
    'profile_revision': 1, 'material_model_id': material['policy']['model_id'],
    'synthetic': True, 'operator_confirmed_claim': False,
    'working_radius_mm': .5, 'wake_length_mm': .65, 'max_top_above_tip_mm': .08,
    'gap_min_mm': .1, 'gap_max_mm': .3, 'width_min_mm': .1,
    'width_max_mm': .55, 'max_path_gradient': .1,
}
cases = [(name, 'PASS') for name in ['complete-two-packets', 'reverse', 'rotated',
         'varying-gap', 'rising', 'descending', 'moving-envelope']]
cases += [(name, 'FAIL') for name in ['current-head', 'static-tip', 'old-neighbor', 'changed-e']]
cases += [(name, 'UNKNOWN') for name in ['partial-block', 'skip-first', 'mixed-block',
          'wrong-role', 'turn', 'reversal', 'confirmation', 'not-synthetic',
          'profile-id', 'profile-revision', 'material-id', 'radius', 'wake',
          'top-depth', 'gap', 'width', 'gradient', 'growth', 'moving-incomplete',
          'query-contact', 'event-contact', 'nested', 'extra', 'version']]
for key, value in contact.items():
    cases += [(f'missing:{key}', 'UNKNOWN'), (f'duplicate:{key}', 'UNKNOWN')]
    if not isinstance(value, bool): cases.append((f'boolean:{key}', 'UNKNOWN'))
records = []
for name, status in cases:
    work = root / name.replace(':', '-')
    work.mkdir()
    m, s, c = copy.deepcopy(material), copy.deepcopy(scene), copy.deepcopy(contact)
    previous, rows, lines = [0, 0, .5], [], ['G90', 'M83', 'M400', 'M204 S4']

    def append(end, e, h0=.2, h1=.2):
        command = 'G1' + ''.join(f' {axis}{value:.9f}' for axis, value in zip('XYZ', end))
        if e: command += f' E{e:.9f}'
        lines.extend([command + ' F30', 'M400'])
        rows.append({'event_id': len(rows) + 1, 'sequence_index': len(rows),
                     'kind': 'deposit' if e else 'travel', 'start': list(previous), 'end': list(end),
                     'expected_nominal_volume_mm3': e * math.pi * rate['filament_diameter'] ** 2 / 4 / rate['flow'],
                     'expected_filament_mm': 0,
                     'section': {'kind': 'rectangle', 'gap_begin_mm': h0, 'gap_end_mm': h1} if e else None})
        previous[:] = end

    def pose(t):
        return [(-1 if name == 'reverse' else 1) * (.6 if name == 'rotated' else 1) * t,
                .8 * t if name == 'rotated' else 0,
                .5 + (.02 * t if name in ['rising', 'gradient'] else -.02 * t if name == 'descending' else 0)]

    first = 0
    if name == 'old-neighbor':
        append(pose(4), .16)
        append(pose(0), 0)
        first = 2
    append(pose(2), .08, .2, .22 if name == 'varying-gap' else .2)
    append([2, 2, .5] if name == 'turn' else pose(0) if name == 'reversal' else pose(4),
           .08, .22 if name == 'varying-gap' else .2, .24 if name == 'varying-gap' else .2)
    # Terminate the maximal Deposit block with real Travel. The later, large
    # Deposit must not become previous material or a negative witness.
    end = list(previous)
    end[2] += .1
    append(end, 0)
    end[0] += 4
    append(end, .16)
    if name == 'current-head': s['head'][4]['min'][2], s['head'][4]['max'][2] = -.15, -.1
    if name == 'static-tip': s['obstacles'] = [{'min': [1.999, .499, .499], 'max': [2.001, .501, .501]}]
    if name in ['moving-envelope', 'moving-incomplete']:
        s['head'][1]['moving'] = True
        s['head'][1]['all_configurations_enclosed'] = name == 'moving-envelope'
    if name == 'confirmation': c['operator_confirmed_claim'] = True
    if name == 'not-synthetic': c['synthetic'] = False
    for case, field in [('profile-id', 'profile_id'), ('profile-revision', 'profile_revision'), ('material-id', 'material_model_id')]:
        if name == case: c[field] += 1
    for case, field, value in [('radius', 'working_radius_mm', .51), ('wake', 'wake_length_mm', .05),
                             ('top-depth', 'max_top_above_tip_mm', .001), ('gap', 'gap_min_mm', .21),
                             ('width', 'width_max_mm', .1), ('gradient', 'max_path_gradient', .001)]:
        if name == case: c[field] = value
    if name == 'growth': m['policy']['outer_z_growth_mm'] = .09
    if name == 'event-contact': rows[first]['contact_model_id'] = 101
    if name == 'nested': c['working_radius_mm'] = {'value': .5}
    if name == 'extra': c['allow_all_material'] = True
    if name == 'version': c['version'] = 2
    q = {'version': 1, 'first_record': first, 'record_count': 2, 'scene': s}
    if name == 'partial-block': q['record_count'] = 1
    if name == 'skip-first': q['first_record'], q['record_count'] = 1, 1
    if name == 'mixed-block': q['record_count'] = 3
    if name == 'wrong-role': q['first_record'], q['record_count'] = 2, 1
    if name == 'query-contact': q['scene']['deposition_contact'] = {'allow': True}
    if name.startswith('missing:'): del c[name.split(':')[1]]
    if name.startswith('boolean:'): c[name.split(':')[1]] = True
    m['events'] = rows
    for filename, value in [('rate.json', rate), ('material.json', m), ('query.json', q), ('contact.json', c)]:
        (work / filename).write_text(json.dumps(value))
    body = '\n'.join(lines) + '\n'
    if name == 'changed-e': body = body.replace('E0.080000000', 'E0.090000000', 1)
    (work / 'candidate.txt').write_text(body)
    if name.startswith('duplicate:'):
        key = name.split(':')[1]
        f = work / 'contact.json'
        f.write_text(f.read_text().replace(json.dumps(key) + ':', json.dumps(key) + ': ' + json.dumps(c[key]) + ', ' + json.dumps(key) + ':', 1))
    files = [work / f for f in ['rate.json', 'material.json', 'query.json', 'contact.json', 'candidate.txt']]
    before = {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
    cmd = [str(exe), '--linear-forming-contact-geometry-only', *map(str, files)]
    run = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
    (work / 'stdout.json').write_bytes(run.stdout)
    (work / 'stderr.txt').write_bytes(run.stderr)
    report = json.loads(run.stdout)
    assert run.returncode == {'PASS': 0, 'FAIL': 2, 'UNKNOWN': 3}[status] and report['component_status'] == status, (name, run.returncode, report)
    assert report['component'] == 'final_byte_forming_contact_geometry' and report['job_status'] == 'UNKNOWN' and report['export_allowed'] is False
    if status == 'PASS': assert report['forming_contact_cells'] > 0 and report['leaves'] > 0 and report['prefix_completed_records'] == first
    if name in ['current-head', 'static-tip', 'old-neighbor']: assert report['witness'] is not None
    if name == 'current-head': assert report['witness']['component'] == 5
    if name == 'old-neighbor': assert report['witness']['material_event'] == 1
    assert before == {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
    records.append({'case': name, 'command': cmd, 'exit_code': run.returncode,
                    'component_status': status, 'inputs_unchanged': True})
(root / 'manifest.json').write_text(json.dumps({'cases': records, 'status': 'PASS',
    'scope': 'DECLARED_RECENT_WORKING_FACE_FORMING_RUN_COMPONENT_ONLY_SUPPORT_PHYSICAL_QUALIFICATION_JOB_EXPORT_BLOCKED'}, indent=2) + '\n')
print(json.dumps({'cases': len(records), 'status': 'PASS', 'job_export': 'BLOCK'}))
