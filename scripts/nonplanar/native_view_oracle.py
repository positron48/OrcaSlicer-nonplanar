#!/usr/bin/env python3
"""Check the adopted public view against a separate native final-byte replay."""
import argparse
import hashlib
import json
from pathlib import Path
from native_cli_gate import check_diagnostic
from job_report_oracle import canonical, require

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--fixtures', type=Path, required=True)
args = parser.parse_args()
view = json.loads((args.fixtures / 'native-view-display-diagnostic.json').read_text())
reference = json.loads((args.fixtures / 'native-cli-reference-diagnostic.json').read_text())
check_diagnostic(view, True)
check_diagnostic(reference, True)
require(view['replay'] == reference['replay'], 'Every original movement/time/volume/byte range equals the separate native owner')
require(view['manifest']['candidate_sha256'] == reference['manifest']['candidate_sha256'], 'Same actual candidate bytes')
request = json.loads((args.fixtures / 'native-controller-request.json').read_text())
# CLI editing JSON is not the internal request identity; compare its replay
# policies through the already independent native diagnostic identity instead.
require(view['request_sha256'] == reference['request_sha256'], 'Same privately captured native request')
require(request['schema'] == 1 and view['completed'], 'Actual version-1 native input')
print(json.dumps({'status': 'PASS_NATIVE_VIEW_DIAGNOSTIC_FINAL_BYTE_REFERENCE',
                  'records': len(view['replay']), 'candidate_sha256': view['report']['validation']['gcode_sha256'],
                  'view_sha256': hashlib.sha256(canonical(view).encode()).hexdigest(),
                  'scope': 'SERIALIZED_DISPLAY_ONLY_NOT_FULL_GUI_OR_PHYSICAL_QUALIFICATION', 'export': 'BLOCK'}))
