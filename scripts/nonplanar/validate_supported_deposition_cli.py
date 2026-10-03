#!/usr/bin/env python3
"""Complete declared forming-block geometry and pre-block underlying support."""
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
rate['initial_position'] = [0, 0, 0]
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
    'version': 2, 'model_id': 101, 'revision': 1, 'profile_id': 51,
    'profile_revision': 1, 'material_model_id': material['policy']['model_id'],
    'synthetic': True, 'operator_confirmed_claim': False,
    'working_radius_mm': .5, 'wake_length_mm': 1.05, 'max_top_above_tip_mm': .08,
    'gap_min_mm': .1, 'gap_max_mm': .3, 'width_min_mm': .1,
    'width_max_mm': .55, 'max_path_gradient': .064, 'min_turn_cosine': 0,
}
support = {
    'version': 1, 'policy_id': 111, 'revision': 1,
    'join': {'version': 1, 'policy_id': 31, 'revision': 1,
             'model': 'common_run_envelope', 'synthetic': True,
             'operator_confirmed_claim': False},
    'runs': [{'first_record': 2, 'last_record': 3, 'policy': {
        'version': 1, 'policy_id': 41, 'revision': 1, 'synthetic': True,
        'operator_confirmed_claim': False, 'cross_slope': .1,
        'vertical_min': .14, 'vertical_max': .26,
        'normal_min': .14, 'normal_max': .26}}],
}
cases = [(name, 'PASS') for name in ['complete-two-packets', 'reverse', 'rotated',
          'rising', 'turn', 'moving-envelope', 'legacy-contact-v1']]
cases += [(name, 'FAIL') for name in ['missing-support', 'future-only-support',
          'thin-support', 'large-gap', 'small-gap', 'rising-cross-anchor', 'current-head', 'static-tip', 'changed-e']]
cases += [(name, 'UNKNOWN') for name in ['partial-block', 'skip-first', 'mixed-block',
          'missing-run', 'duplicate-run', 'partial-run', 'future-run', 'reversed-run',
          'swapped-turn-runs', 'unlisted-turn', 'wrong-join', 'join-confirmed',
          'support-confirmed', 'support-not-synthetic', 'negative-gap', 'reversed-gap',
          'nested', 'extra', 'extra-run', 'extra-policy', 'version']]
objects = [('', support), ('join', support['join']), ('run', support['runs'][0]),
           ('policy', support['runs'][0]['policy'])]
for section, obj in objects:
    for key, value in obj.items():
        for mutation in ['missing', 'duplicate', 'boolean']:
            # Boolean ownership fields already have a boolean type. Their
            # unsupported true/non-synthetic values have dedicated cases.
            if mutation == 'boolean' and isinstance(value, bool): continue
            cases.append((f'{mutation}:{section}:{key}', 'UNKNOWN'))

records = []
for name, status in cases:
    work = root / name.replace(':', '-')
    work.mkdir()
    m, s, c, policy = map(copy.deepcopy, [material, scene, contact, support])
    previous, rows, lines = [0, 0, 0], [], ['G90', 'M83', 'M400', 'M204 S4']

    def append(end, e):
        command = 'G1' + ''.join(f' {axis}{v:.9f}' for axis, v in zip('XYZ', end))
        if e: command += f' E{e:.9f}'
        lines.extend([command + ' F30', 'M400'])
        rows.append({'event_id': len(rows) + 1, 'sequence_index': len(rows),
                     'kind': 'deposit' if e else 'travel', 'start': list(previous),
                     'end': list(end), 'expected_nominal_volume_mm3':
                     e * math.pi * rate['filament_diameter'] ** 2 / 4 / rate['flow'],
                     'expected_filament_mm': 0, 'section':
                     {'kind': 'rectangle', 'gap_begin_mm': .2, 'gap_end_mm': .2} if e else None})
        previous[:] = end

    def pose(t, z):
        return [(-1 if name == 'reverse' else 1) * (.6 if name == 'rotated' else 1) * t,
                .8 * t if name == 'rotated' else 0, z]

    height = .5 if name == 'large-gap' else .05 if name == 'small-gap' else .2
    append(pose(3, 0), 0 if name in ['missing-support', 'future-only-support'] else .0001 if name == 'thin-support' else .4)
    append(pose(.5, height), 0)
    rising = name in ['rising', 'rising-cross-anchor']
    if name == 'rising': policy['runs'][0]['policy']['cross_slope'] = 0
    append(pose(1.5, height + (.01 if rising else 0)), .04)
    turning = name in ['turn', 'swapped-turn-runs', 'unlisted-turn']
    append([1.5, .3, height] if turning else pose(2.5, height + (.02 if rising else 0)), .012 if turning else .04)
    if turning:
        policy['runs'][0]['last_record'] = 2
        policy['runs'][0]['policy']['cross_slope'] = 0
        second = copy.deepcopy(policy['runs'][0])
        second.update(first_record=3, last_record=3)
        second['policy']['policy_id'] = 42
        policy['runs'].append(second)
    # Real block delimiter, followed by large future material. The combined
    # proof must neither use that material for support nor inspect it as old.
    append([0, 0, 0], 0)
    append([3, 0, 0], .4)
    query = {'version': 1, 'first_record': 2, 'record_count': 2, 'scene': s}
    if name == 'partial-block': query['record_count'] = 1
    if name == 'skip-first': query.update(first_record=3, record_count=1)
    if name == 'mixed-block': query['record_count'] = 3
    if name == 'missing-run': policy['runs'] = []
    if name == 'duplicate-run': policy['runs'].append(copy.deepcopy(policy['runs'][0]))
    if name == 'partial-run': policy['runs'][0]['last_record'] = 2
    if name == 'future-run': policy['runs'][0].update(first_record=5, last_record=5)
    if name == 'reversed-run': policy['runs'][0].update(first_record=3, last_record=2)
    if name == 'swapped-turn-runs': policy['runs'].reverse()
    if name == 'unlisted-turn': policy['runs'].pop()
    if name == 'wrong-join': policy['join']['model'] = 'unqualified_union'
    if name == 'join-confirmed': policy['join']['operator_confirmed_claim'] = True
    if name == 'support-confirmed': policy['runs'][0]['policy']['operator_confirmed_claim'] = True
    if name == 'support-not-synthetic': policy['runs'][0]['policy']['synthetic'] = False
    if name == 'negative-gap': policy['runs'][0]['policy']['vertical_min'] = -.1
    if name == 'reversed-gap': policy['runs'][0]['policy']['normal_min'] = .3
    if name == 'nested': policy['policy_id'] = {'value': 111}
    if name == 'extra': policy['allow_old_material'] = True
    if name == 'extra-run': policy['runs'][0]['certificate'] = 'PASS'
    if name == 'extra-policy': policy['runs'][0]['policy']['exclude_formed_material'] = True
    if name == 'version': policy['version'] = 2
    if name == 'legacy-contact-v1': c['version'] = 1; del c['min_turn_cosine']
    if name == 'current-head': s['head'][4]['min'][2], s['head'][4]['max'][2] = -.15, -.1
    if name == 'static-tip': s['obstacles'] = [{'min': [1.499, .499, .199], 'max': [1.501, .501, .201]}]
    if name == 'moving-envelope': s['head'][1].update(moving=True, all_configurations_enclosed=True)
    duplicate = None
    if ':' in name:
        mutation, section, key = name.split(':')
        obj = {'': policy, 'join': policy['join'], 'run': policy['runs'][0],
               'policy': policy['runs'][0]['policy']}[section]
        if mutation == 'missing': del obj[key]
        elif mutation == 'boolean': obj[key] = True
        else: duplicate = (obj, key)
    m['events'] = rows
    for filename, value in [('rate.json', rate), ('material.json', m), ('query.json', query),
                            ('contact.json', c), ('support.json', policy)]:
        text = json.dumps(value)
        if filename == 'support.json' and duplicate:
            obj, key = duplicate
            object_text = json.dumps(obj)
            at = text.index(object_text) + len(object_text) - 1
            text = text[:at] + ', ' + json.dumps(key) + ': ' + json.dumps(obj[key]) + text[at:]
        (work / filename).write_text(text)
    body = '\n'.join(lines) + '\n'
    if name == 'changed-e': body = body.replace('E0.040000000', 'E0.050000000', 1)
    (work / 'candidate.txt').write_text(body)
    files = [work / f for f in ['rate.json', 'material.json', 'query.json', 'contact.json', 'support.json', 'candidate.txt']]
    before = {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
    cmd = [str(exe), '--linear-supported-deposition-only', *map(str, files)]
    run = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
    (work / 'stdout.json').write_bytes(run.stdout)
    (work / 'stderr.txt').write_bytes(run.stderr)
    report = json.loads(run.stdout)
    assert run.returncode == {'PASS': 0, 'FAIL': 2, 'UNKNOWN': 3}[status] and report['component_status'] == status, (name, run.returncode, report)
    assert report['component'] == 'final_byte_supported_deposition' and report['job_status'] == 'UNKNOWN' and report['export_allowed'] is False
    assert report['scope'] == 'complete_declared_forming_block_head_contact_and_pre_block_underlying_gap_lower_anchor_only'
    if status == 'PASS':
        assert report['support_runs'] == len(policy['runs']) and report['cells'] > report['geometry_cells'] > 0
        assert report['underlying_completed_records'] == 2
        assert report['geometry_witness'] is None and report['support_witness'] is None
    if name in ['missing-support', 'future-only-support', 'thin-support', 'large-gap', 'small-gap', 'rising-cross-anchor']:
        assert report['support_witness'] is not None and report['failed_run'] == 0 and report['geometry_witness'] is None
    if name in ['current-head', 'static-tip']: assert report['geometry_witness'] is not None and report['support_witness'] is None
    if status == 'UNKNOWN': assert report['geometry_witness'] is None and report['support_witness'] is None
    assert before == {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in files}
    records.append({'case': name, 'command': cmd, 'exit_code': run.returncode,
                    'component_status': status, 'inputs_unchanged': True})
(root / 'manifest.json').write_text(json.dumps({'cases': records, 'status': 'PASS',
    'scope': 'COMPLETE_DECLARED_BLOCK_GEOMETRY_PRE_BLOCK_UNDERLYING_SUPPORT_ONLY_FULL_JOB_EXPORT_BLOCK'}, indent=2) + '\n')
print(json.dumps({'cases': len(records), 'status': 'PASS', 'job_export': 'BLOCK'}))
