#!/usr/bin/env python3
"""Exercise the real Orca native analysis action in fresh offline data directories."""
import argparse
import copy
import datetime
import hashlib
import json
import os
from pathlib import Path
import signal
import selectors
import subprocess
import sys
import time

from job_report_oracle import canonical, require


def sha(text):
    return hashlib.sha256(text.encode()).hexdigest()


def check_diagnostic(value, complete):
    require(value['schema'] == 1 and value['completed'] == complete and
            value['export_allowed'] is False and 'candidate_bytes' not in value, 'Blocked diagnostic envelope')
    if not complete:
        require(value['report'] is None and value['replay'] == [], 'No partial published result')
        return
    report = value['report']
    require(value['report_sha256'] == sha(canonical(report)), 'Exact report hash')
    require(value['manifest_sha256'] == sha(canonical(value['manifest'])) and
            report['manifest_sha256'] == value['manifest_sha256'], 'Exact manifest binding')
    v = report['validation']
    require(v['export_decision'] == 'BLOCK' and v['overall_status'] == 'UNKNOWN', 'Mandatory gate')
    require(len(v['checks']) == 17 and sum(c['status'] == 'PASS' and c['execution'] == 'RUN' for c in v['checks']) == 4 and
            sum(c['status'] == 'UNKNOWN' and c['execution'] == 'NOT_RUN' for c in v['checks']) == 13, 'Fixed mandatory registry')
    require(len(value['replay']) == report['replay']['records'] > 1000, 'Actual full movement replay')
    previous_end = None
    for index, row in enumerate(value['replay']):
        require(row['index'] == index and len(row['start_mm']) == len(row['end_mm']) == 3, 'Replay index/axes')
        require(previous_end is None or row['start_mm'] == previous_end, 'Replay continuous position')
        require(row['candidate_byte_range'][0] < row['candidate_byte_range'][1], 'Movement byte interval')
        require(0 <= row['duration_s'][0] <= row['duration_s'][1] and
                0 <= row['nominal_volume_mm3'][0] <= row['nominal_volume_mm3'][1], 'Outward replay bounds')
        previous_end = row['end_mm']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--evidence-dir', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    binary = args.binary.resolve()
    fixtures = args.fixtures.resolve()
    evidence = args.evidence_dir.resolve()
    evidence.mkdir(parents=True, exist_ok=False)
    original = json.loads((fixtures / 'native-controller-request.json').read_text())
    config = json.loads((fixtures / 'native-cli-config.json').read_text())
    option_keys = json.loads((fixtures / 'native-cli-option-keys.json').read_text())
    reference = json.loads((fixtures / 'native-cli-reference-diagnostic.json').read_text())
    model = root / 'tests/nonplanar/data/affine-wedge-1-in-16.stl'
    cases = [('complete-blocked', original, config, True, [], False)]
    changed = copy.deepcopy(original)
    changed['millimeters_declared'] = False
    cases.append(('units-unconfirmed', changed, config, False, [], False))
    changed = copy.deepcopy(original)
    changed['motion']['operator_confirmed_claim'] = True
    cases.append(('unsupported-qualification', changed, config, False, [], False))
    cases.append(('duplicate-key', '{"schema":1,' + json.dumps(original)[1:], config, False, [], False))
    cases.append(('oversized-request', ' ' * (2 * 1024 * 1024 + 1), config, False, [], False))
    changed_config = copy.deepcopy(config)
    changed_config['nptop_mode'] = 'off'
    cases.append(('off-refused', original, changed_config, False, [], False))
    cases.append(('mixed-actions-refused', original, config, None, ['--export-settings', 'should-not-exist.json'], False))
    cases.append(('missing-datadir-refused', original, config, None, [], False))
    changed_config = copy.deepcopy(config)
    changed_config['machine_start_gcode'] = 'G28 ; fixture secret must not enter diagnostics'
    cases.append(('custom-code-refused', original, changed_config, False, [], False))
    if os.name != 'nt':
        cases.append(('interrupt-native-body', original, config, False, [], True))
    manifest = {'schema': 1, 'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(), 'cases': [],
                'scope': 'REAL_ORCA_NATIVE_ANALYSIS_DIAGNOSTICS_ONLY', 'export': 'BLOCK'}
    for name, request, settings, complete, extra, interrupt in cases:
        work = evidence / name
        work.mkdir()
        for folder in ['data', 'tmp', 'output']:
            (work / folder).mkdir()
        request_path = work / 'request.json'
        request_path.write_text(request if isinstance(request, str) else json.dumps(request))
        configs = {}
        for kind in ['machine', 'process', 'filament']:
            value = {key: settings[key] for key in option_keys[kind] if key in settings}
            value.update(type=kind, name='NPTOP CLI simulation '+kind, version='2.4.2.0', inherits='', **{'from': 'system'})
            if kind == 'machine':
                value['printer_model'] = 'NPTOP simulation'
                value['printer_variant'] = '0.4'
            if kind == 'process':
                value['nptop_mode'] = settings['nptop_mode']
            value['compatible_printers'] = ['NPTOP CLI simulation machine']
            if kind == 'filament':
                # Orca's per-filament merge reads vector element zero. Empty
                # editing vectors are omitted from this one-filament override.
                # Explicit empty code entries suppress stock's whitespace hook
                # defaults while retaining the request's no-custom-code policy.
                for key in ['filament_start_gcode', 'filament_end_gcode', 'filament_change_extrusion_role_gcode']:
                    if value.get(key) == []:
                        value[key] = ['']
                value = {key: item for key, item in value.items() if item != []}
            path = work / (kind+'.json')
            path.write_text(json.dumps(value))
            configs[kind] = str(path)
        command = [str(binary), '--datadir', str(work / 'data'), '--debug', '2', '--load-settings', configs['machine']+';'+configs['process'],
                   '--load-filaments', configs['filament'], '--arrange', '1', '--orient', '0', '--outputdir', str(work / 'output'),
                   '--filament-map-mode', settings['filament_map_mode'],
                   '--enable-filament-dynamic-map=0', '--has-filament-switcher=0',
                   '--nptop-analyze', str(request_path), *extra, str(model)]
        if name == 'missing-datadir-refused':
            index = command.index('--datadir')
            del command[index:index+2]
        if sys.platform == 'darwin':
            policy = work / 'offline.sb'
            policy.write_text('(version 1)\n(allow default)\n(deny network*)\n(deny file-write*)\n'
                              f'(allow file-write* (subpath {json.dumps(str(work))}))\n'
                              '(allow file-write* (literal "/dev/null"))\n')
            command = ['sandbox-exec', '-f', str(policy), *command]
        environment = dict(os.environ, TMPDIR=str(work / 'tmp') + os.sep)
        start = time.monotonic()
        entry = {'name': name, 'command': command, 'cwd': str(work),
                 'started_utc': datetime.datetime.now(datetime.timezone.utc).isoformat()}
        with (work / 'stdout.txt').open('wb') as out, (work / 'stderr.txt').open('wb') as err:
            if interrupt:
                process = subprocess.Popen(command, cwd=work, env=environment, stdin=subprocess.DEVNULL,
                                           stdout=out, stderr=subprocess.PIPE)
                sent, tail = False, b''
                with selectors.DefaultSelector() as poll:
                    poll.register(process.stderr, selectors.EVENT_READ)
                    while poll.get_map():
                        if time.monotonic()-start > 45:
                            process.kill()
                            process.wait()
                            raise ValueError('Native CLI interruption deadline')
                        for key, _ in poll.select(.1):
                            data = key.fileobj.read1(65536)
                            if not data:
                                poll.unregister(key.fileobj)
                                continue
                            err.write(data)
                            tail += data
                            if not sent and b'NPTOP_PROGRESS body\n' in tail:
                                process.send_signal(signal.SIGINT)
                                sent = True
                            tail = tail[-1024:]
                code = process.wait(timeout=45)
                require(sent, 'Actual native body progress observed before interruption')
            else:
                code = subprocess.run(command, cwd=work, env=environment, stdin=subprocess.DEVNULL,
                                      stdout=out, stderr=err, timeout=45).returncode
        entry.update(status='COMPLETED', exit_code=code, elapsed_seconds=round(time.monotonic()-start, 3))
        (work / 'command.json').write_text(json.dumps(entry, indent=2) + '\n')
        require(code > 0, 'Mandatory refusal has positive nonzero exit, no signal crash')
        require(not list(work.rglob('*.gcode')) and not (work / 'should-not-exist.json').exists(), 'No G-code or mixed-action output')
        lines = [line for line in (work / 'stdout.txt').read_text().splitlines() if line.startswith('NPTOP_DIAGNOSTIC ')]
        if complete is None:
            require(not lines and b'NPTOP_PROGRESS' not in (work / 'stderr.txt').read_bytes(), 'Mixed actions refuse before execution')
        else:
            require(len(lines) == 1, 'Exactly one complete diagnostic record')
            diagnostic = json.loads(lines[0].removeprefix('NPTOP_DIAGNOSTIC '))
            (work / 'diagnostic.json').write_text(json.dumps(diagnostic, indent=2) + '\n')
            check_diagnostic(diagnostic, complete)
            entry.update(stage=diagnostic['stage'], reason=diagnostic['reason'])
            if complete:
                stages = [line.removeprefix('NPTOP_PROGRESS ') for line in (work / 'stderr.txt').read_text().splitlines() if line.startswith('NPTOP_PROGRESS ')]
                require(stages == ['capture', 'body', 'hatches', 'cap', 'material', 'motion', 'serialize', 'lineage', 'replay', 'admission'], 'All native stages invoked')
                require(diagnostic['request_sha256'] == reference['request_sha256'], 'Same explicit owned request')
                require(diagnostic['report']['validation']['gcode_sha256'] == reference['report']['validation']['gcode_sha256'] and
                        diagnostic['replay'] == reference['replay'], 'Actual CLI candidate and every replay row match the original native fixture')
            if interrupt:
                require(diagnostic['reason'] == 'NATIVE_ANALYSIS_CANCELLED', 'Cooperative signal cancellation')
            if name == 'custom-code-refused':
                require(diagnostic['reason'] == 'NATIVE_ANALYSIS_POLICY_REFUSED:machine_start_gcode' and
                        'fixture secret' not in lines[0], 'Specific refusal without raw hook content')
        manifest['cases'].append(entry)
        (evidence / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        print(json.dumps({'case': name, 'status': 'PASS', 'exit_code': code}), flush=True)
    print(json.dumps({'status': 'PASS', 'cases': len(cases), 'export': 'BLOCK'}))


if __name__ == '__main__':
    main()
