#!/usr/bin/env python3
"""Original native policy, unchanged XYZ/E and strict historical-byte refusal."""
import argparse
from decimal import Decimal, localcontext
import gzip
import hashlib
import json
from pathlib import Path
import re
import subprocess

from job_report_oracle import parse, require, verify as verify_report
from native_job_inputs_oracle import verify_inputs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--parent-report', type=Path, required=True)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=False)
    record = parse((args.fixtures / 'native-later-original-policy-report.json').read_text())
    with gzip.open(args.parent_report, 'rt') as stream:
        parent = parse(stream.read())
    verify_report(record)
    verify_report(parent)
    verify_inputs(record)
    verify_inputs(parent)
    geometry = lambda text: re.sub(r' F[^\n ]+', '', re.sub(r'M204 S[^\n]+\n', '', text))
    require(geometry(record['candidate_bytes']) == geometry(parent['candidate_bytes']),
            'Every original XYZ/E/order/barrier unchanged')
    require(record['initial_position'] == parent['initial_position'], 'Actual initial binary pose unchanged')
    acceleration = Decimal(re.search(r'M204 S([^\n]+)', record['candidate_bytes'])[1])
    require(acceleration == Decimal('85.530744'), 'Minimal native decimal acceleration reduction')
    # The original finite decimal connector is an independent analytic witness.
    moves = [line for line in parent['candidate_bytes'].splitlines() if line.startswith(('G1 ', 'G4 '))]
    position = [Decimal.from_float(v) for v in parent['initial_position']]
    excess = None
    with localcontext() as context:
        context.prec = 80
        for index, line in enumerate(moves):
            fields = dict(re.findall(r'([XYZEF])([^ ]+)', line))
            if 'X' not in fields:
                continue
            end = [Decimal(fields[axis]) for axis in 'XYZ']
            if index == 1754:
                delta = [b-a for a, b in zip(position, end)]
                length = sum(v*v for v in delta).sqrt()
                old = abs(delta[2])*Decimal('85.530745')/length
                new = abs(delta[2])*acceleration/length
                require(old > 50 and new < 50, 'Original Z50 exact-decimal exceedance and correction')
                excess = {'record': index, 'old_z_acceleration': str(old), 'new_z_acceleration': str(new)}
            position = end
    require(excess is not None, 'Original connector witness retained')
    policy = {'version': 1, 'profile_id': 91, 'revision': 1, 'synthetic': True,
              'operator_confirmed_claim': False, 'model': 'full_stop', 'kinematics': 'corexy',
              'initial_position': record['initial_position'], 'position_min': [-1, -1, -.1],
              'position_max': [40, 40, 10], 'axis_speed': [200, 200, 5],
              'axis_acceleration': [1000, 1000, 50], 'drive_speed': [200, 200, 5],
              'drive_acceleration': [1000, 1000, 50], 'initial_acceleration': 100,
              'filament_diameter': 1.75, 'flow': 1, 'filament_speed': 40,
              'filament_acceleration': 400, 'max_retraction': 5, 'max_volume_rate': 12,
              'max_cross_section': 2, 'max_event_rate': 200}
    profile = args.output_dir / 'policy.json'
    profile.write_text(json.dumps(policy, indent=2)+'\n')
    cases = []
    mutated = re.sub(r'M204 S[^\n]+', 'M204 S85.530745', record['candidate_bytes'])
    for name, text, status, code in [('retuned', record['candidate_bytes'], 'PASS', 0),
                                    ('historical', parent['candidate_bytes'], 'FAIL', 2),
                                    ('restored-unsafe-acceleration', mutated, 'FAIL', 2)]:
        path = args.output_dir / (name+'.candidate.txt')
        path.write_text(text)
        command = [str(args.executable.resolve()), '--linear-rates-only', str(profile.resolve()), str(path.resolve())]
        result = subprocess.run(command, capture_output=True, timeout=10)
        (args.output_dir / (name+'.stdout.json')).write_bytes(result.stdout)
        (args.output_dir / (name+'.stderr.txt')).write_bytes(result.stderr)
        output = parse(result.stdout.decode())
        require(result.returncode == code and output['component_status'] == status and
                output['job_status'] == 'UNKNOWN' and output['export_allowed'] is False,
                'Actual independent native rate auditor result')
        if code:
            require(output['reason'] == 'AXIS_ACCELERATION_LIMIT', 'Historical unsafe bytes still refused')
        cases.append({'name': name, 'command': command, 'exit_code': result.returncode,
                      'candidate_sha256': hashlib.sha256(text.encode()).hexdigest(), 'report': output})
    report = {'status': 'PASS', 'scope': 'NATIVE_DECIMAL_RATES_AND_UNCHANGED_XYZ_E_ONLY',
              'original_policy': 100, 'acceleration': str(acceleration), 'witness': excess,
              'cases': cases, 'job_status': 'UNKNOWN', 'export': 'BLOCK'}
    (args.output_dir / 'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
