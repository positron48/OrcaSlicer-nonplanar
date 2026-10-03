from pathlib import Path
from decimal import Decimal, localcontext
import hashlib, json, re

root = Path.cwd()
raw = root / 'build/nonplanar-evidence/B14-native-gui'
reference = json.loads((raw / 'ctest3-jobs/native-cli-reference-diagnostic.json').read_text())
state = (raw / 'gui-final-completed.txt').read_text()
report = json.loads(re.search(r'\d+ text entry area (\{.*?\n\})', state, re.S).group(1))
assert 'WORKER_BLOCKED_DIAGNOSTIC_COMPLETE' in state
assert report['export_allowed'] is False and report['report']['replay']['records'] == 2098
validation = report['report']['validation']
assert validation['gcode_sha256'] == reference['report']['validation']['gcode_sha256']
assert validation['export_decision'] == 'BLOCK' and validation['overall_status'] == 'UNKNOWN'
checks = validation['checks']
assert len(checks) == 17 and sum(c['status'] == 'PASS' and c['execution'] == 'RUN' for c in checks) == 4
assert sum(c['status'] == 'UNKNOWN' and c['execution'] == 'NOT_RUN' for c in checks) == 13
assert validation['mandatory_check_ids'] == reference['report']['validation']['mandatory_check_ids']

for name, index in [('gui-final-completed.txt', 0), ('gui-final-last.txt', 2097)]:
    text = (raw / name).read_text()
    row = reference['replay'][index]
    match = re.search(r'Movement (\d+)/2097  Deposit  t=(\[[^\n]+?\]) s\nXYZ (\[.*?\]) → (\[.*?\])  E=(\S+) mm  F=(\S+) mm/min\nNominal volume (\[.*?\]) mm³  bytes (\[.*?\])', text)
    assert match and int(match[1]) == index
    for captured, field in [(3, 'start_mm'), (4, 'end_mm'), (5, 'e_mm'), (6, 'feed_mm_min'), (7, 'nominal_volume_mm3'), (8, 'candidate_byte_range')]:
        assert json.loads(match[captured]) == row[field], (name, field)
    shown = json.loads(match[2], parse_float=Decimal)
    with localcontext() as context:
        context.prec = 130
        actual = [sum(Decimal.from_float(float(m['duration_s'][side])) for m in reference['replay'][:index + 1]) for side in [0, 1]]
        assert shown[0] <= actual[0] <= actual[1] <= shown[1], (name, shown, actual)

for name, reason in [('gui-final-edit.txt', 'Editing — previous analysis invalidated'), ('gui-final-cancel.txt', 'VIEW_CANCELLED')]:
    text = (raw / name).read_text()
    assert reason in text and 'slider (disabled)' in text and 'Movement replay: unavailable' in text
    assert 'gcode_sha256' not in text and 'export_allowed' not in text
assert 'standard window simulation-identity-source' in (raw / 'gui-final-close.txt').read_text()
menu = (raw / 'gui-final-idle-menu.txt').read_text()
assert '12 Nonplanar analysis…' in menu and '(disabled) Export G-code' in menu

sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
bundle = json.loads((raw / 'bundle-delivery-manifest.json').read_text())
assert bundle['binary_sha256'] == sha(root / 'build/arm64/src/Release/OrcaSlicer.app/Contents/MacOS/OrcaSlicer')
assert bundle['worker_sha256'] == sha(root / 'build/arm64/src/nonplanar_analysis/Release/nonplanar_analysis_worker')
assert sha(Path(bundle['destination']) / 'Contents/MacOS/NonplanarTopLabCore') == bundle['binary_sha256']
assert sha(Path(bundle['destination']) / 'Contents/MacOS/nonplanar_analysis_worker') == bundle['worker_sha256']
result = {'status': 'PASS_OBSERVED_NATIVE_GUI_WITH_FINAL_BINARY_WORKER_BINDING', 'binary_sha256': bundle['binary_sha256'],
          'worker_sha256': bundle['worker_sha256'], 'records': 2098, 'candidate_sha256': validation['gcode_sha256'],
          'first_last_original_movements_exact': True, 'rendered_cumulative_intervals_enclose_reference': True,
          'edit_cancel_close_idle_queue': 'PASS_OBSERVED', 'mandatory': {'pass_run': 4, 'unknown_not_run': 13},
          'worker_peak_rss_bytes': report['worker_peak_rss_bytes'], 'export': 'BLOCK',
          'scope': 'MINIMAL_SYNTHETIC_NATIVE_GUI_NOT_FULL_3MF_OR_FULL_P2_OR_PHYSICAL', 'independent_review': 'PENDING'}
(raw / 'gui-observation.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result))
