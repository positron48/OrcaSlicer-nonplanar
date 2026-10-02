"""Reproduce rehashed identity/order refusals; no geometry/authenticity claim."""
import copy, hashlib, importlib.util, json, pathlib, struct
root = pathlib.Path.cwd()
raw = root / 'build/nonplanar-evidence/B13-native-departure'
fixtures = raw / 'focus-evidence'
spec = importlib.util.spec_from_file_location('oracle', root / 'scripts/nonplanar/job_report_oracle.py')
oracle = importlib.util.module_from_spec(spec); spec.loader.exec_module(oracle)
canonical = oracle.canonical
sha = oracle.sha
bits = lambda value: struct.pack('>d', value).hex()
cases = []
def run(name, record, success):
    reason = None
    try:
        answer = oracle.verify(record)
    except (ValueError, KeyError, TypeError, IndexError, OverflowError, struct.error) as error:
        reason = str(error) or type(error).__name__
    assert (reason is None) == success, (name, reason)
    cases.append({'name': name, 'expected': 'PASS' if success else 'REFUSE',
                  'reason': reason, 'input_sha256': sha(canonical(record))})

def synchronize(record):
    n = record['native']
    body, hatch, plan = (json.loads(n[k]) for k in ['body_canonical', 'hatch_canonical', 'canonical'])
    for name in ['body', 'assembled', 'planned', 'before', 'routed']:
        journal = n[name]; context = json.loads(journal['context'])
        context['record_count'] = len(journal['records']); journal['context'] = canonical(context)
        digest = sha('nptop-material-ledger-v1\0' + journal['context'])
        for text in journal['records']:
            digest = sha('nptop-material-record-v1\0' + digest + text)
        journal['sha256'] = digest
    body['body_journal'] = n['body']['sha256']
    n['body_canonical'] = canonical(body); n['body_sha256'] = sha(n['body_canonical'])
    hatch['body_lineage'] = n['body_sha256']
    n['hatch_canonical'] = canonical(hatch); n['hatch_sha256'] = sha(n['hatch_canonical'])
    departure = json.loads(n['departure_canonical'])
    for field, name in [('before_journal', 'before'), ('laid_journal', 'assembled'), ('routed_journal', 'routed')]:
        departure[field] = n[name]['sha256'].encode().hex()
    n['departure_canonical'] = canonical(departure); n['departure_sha256'] = sha(n['departure_canonical'])
    plan.update(hatch_lineage=n['hatch_sha256'], assembled_journal=n['assembled']['sha256'],
                planned_journal=n['planned']['sha256'], departure_lineage=n['departure_sha256'])
    n['canonical'] = canonical(plan); n['sha256'] = sha(n['canonical'])
    record['material_journal'] = n['planned']['sha256']
    manifest = json.loads(record['manifest'])
    manifest.update(native_lineage=n['sha256'].encode().hex(), material_journal=record['material_journal'].encode().hex())
    record['manifest'] = canonical(manifest); record['manifest_sha256'] = sha(record['manifest'])
    report = json.loads(record['canonical']); report['manifest_sha256'] = record['manifest_sha256']
    record['canonical'] = canonical(report); record['sha256'] = sha(record['canonical'])

for name in ['candidate-report', 'candidate-report-invalid-material', 'candidate-report-failed-material',
             'native-lineage-report', 'native-departure-report']:
    run(name, json.loads((fixtures / (name + '.json')).read_text()), True)
base = json.loads((fixtures / 'native-departure-report.json').read_text())
def change_departure(record, mutate):
    n = record['native']; document = json.loads(n['departure_canonical']); mutate(document)
    n['departure_canonical'] = canonical(document)
def change_row(record, journal, index, mutate):
    rows = record['native'][journal]['records']; row = json.loads(rows[index]); mutate(row); rows[index] = canonical(row)
def change_context(record, journal, mutate):
    n = record['native'][journal]; document = json.loads(n['context']); mutate(document); n['context'] = canonical(document)
mutations = {
    'origin-map-missing': lambda d: change_departure(d, lambda p: p['source_records'].pop()),
    'origin-map-foreign-parent': lambda d: change_departure(d, lambda p: p['source_records'].__setitem__(-1, 0)),
    'origin-map-boolean': lambda d: change_departure(d, lambda p: p['source_records'].__setitem__(0, False)),
    'leg-missing': lambda d: change_departure(d, lambda p: p['legs'].pop()),
    'leg-reordered': lambda d: change_departure(d, lambda p: p['legs'].reverse()),
    'leg-boolean-cells': lambda d: change_departure(d, lambda p: p['legs'][0].__setitem__(1, True)),
    'leg-unbounded': lambda d: change_departure(d, lambda p: p['legs'][0].__setitem__(1, 1000001)),
    'request-wrong-destination': lambda d: change_departure(d, lambda p: p['request'][0].__setitem__(0, bits(30))),
    'request-wrong-lift': lambda d: change_departure(d, lambda p: p['request'].__setitem__(1, bits(8))),
    'request-negative-speed': lambda d: change_departure(d, lambda p: p['request'].__setitem__(2, bits(-1))),
    'routed-missing-descent': lambda d: d['native']['routed']['records'].pop(),
    'routed-foreign-order': lambda d: change_row(d, 'routed', -1, lambda p: p['motion'].__setitem__(1, 0)),
    'routed-reused-event-id': lambda d: change_row(d, 'routed', -1, lambda p: p['motion'].__setitem__(0, 1)),
    'routed-wrong-pose': lambda d: change_row(d, 'routed', -1, lambda p: p['motion'][4].__setitem__(0, bits(30))),
    'routed-phantom-deposition': lambda d: change_row(d, 'routed', -1, lambda p: p['motion'].__setitem__(7, [1, bits(.1)])),
    'routed-changed-old-row': lambda d: change_row(d, 'routed', 0, lambda p: p['motion'].__setitem__(0, 5)),
    'laid-changed-old-row': lambda d: change_row(d, 'assembled', 0, lambda p: p['motion'].__setitem__(0, 5)),
    'before-body-prefix': lambda d: change_row(d, 'before', 0, lambda p: p['motion'].__setitem__(0, 5)),
    'before-source': lambda d: change_context(d, 'before', lambda p: p.update(source='foreign'.encode().hex())),
    'laid-reduced-error': lambda d: change_context(d, 'assembled', lambda p: p['model'].__setitem__(5, bits(0))),
    'routed-reduced-error': lambda d: change_context(d, 'routed', lambda p: p['model'].__setitem__(5, bits(0))),
    'planned-extra-row': lambda d: d['native']['planned']['records'].append(d['native']['planned']['records'][-1]),
    'planned-speed-increase': lambda d: change_row(d, 'planned', -1, lambda p: p['motion'].__setitem__(5, bits(1000))),
    'planned-pose-change': lambda d: change_row(d, 'planned', -1, lambda p: p['motion'][4].__setitem__(0, bits(30))),
    'clearance-negative': lambda d: change_departure(d, lambda p: p['clearance'].__setitem__(0, bits(-.01))),
    'clearance-nonfinite': lambda d: change_departure(d, lambda p: p['clearance'].__setitem__(0, bits(float('inf')))),
    'clearance-component-missing': lambda d: change_departure(d, lambda p: p['clearance'].pop()),
    'head-missing-role': lambda d: change_departure(d, lambda p: p['scene']['head'].pop()),
    'head-reused-id': lambda d: change_departure(d, lambda p: p['scene']['head'][1].__setitem__(0, p['scene']['head'][0][0])),
    'head-unknown-role': lambda d: change_departure(d, lambda p: p['scene']['head'][0].__setitem__(1, 6)),
    'head-inverted-box': lambda d: change_departure(d, lambda p: p['scene']['head'][0][2][0].__setitem__(0, bits(100))),
    'annulus-confused-diameter': lambda d: change_departure(d, lambda p: p['scene']['tip'].__setitem__(2, p['scene']['tip'][1])),
    'inventory-incomplete': lambda d: change_departure(d, lambda p: p['scene']['coverage'].__setitem__(3, False)),
    'operator-confirmation-forged': lambda d: change_departure(d, lambda p: p['scene']['identity'].__setitem__(4, True)),
    'departure-unknown-version': lambda d: change_departure(d, lambda p: p.update(schema=2)),
    'departure-boolean-version': lambda d: change_departure(d, lambda p: p.update(schema=True)),
    'departure-full-scope-claim': lambda d: change_departure(d, lambda p: p.update(scope='qualified_whole_job')),
    'departure-extra-field': lambda d: change_departure(d, lambda p: p.update(clearance_verified=True)),
}
for name, mutate in mutations.items():
    record = copy.deepcopy(base); mutate(record); synchronize(record); run(name, record, False)
for name in ['manifest-downgrade', 'missing-owner', 'extra-native-claim', 'forged-complete-order', 'export-allow', 'stale-attempt']:
    record = copy.deepcopy(base); synchronize(record)
    if name == 'manifest-downgrade':
        document = json.loads(record['manifest']); document.update(schema=2, scope='owned_native_body_cap_candidate_lineage_only')
        record['manifest'] = canonical(document); record['manifest_sha256'] = sha(record['manifest'])
        document = json.loads(record['canonical']); document['manifest_sha256'] = record['manifest_sha256']
    else:
        document = json.loads(record['canonical'])
        if name == 'missing-owner': record['native'].pop('departure_canonical')
        elif name == 'extra-native-claim': record['native']['whole_job_verified'] = True
        elif name == 'forged-complete-order':
            check = next(c for c in document['validation']['checks'] if c['id'] == 'complete_route_order')
            check.update(status='PASS', execution='RUN')
        elif name == 'export-allow': document['validation']['export_decision'] = 'ALLOW'
        else: record['attempt'] += 1
    record['canonical'] = canonical(document); record['sha256'] = sha(record['canonical']); run(name, record, False)
result = {'status': 'PASS', 'positives': sum(c['expected'] == 'PASS' for c in cases),
          'refusals': sum(c['expected'] == 'REFUSE' for c in cases),
          'scope': 'IDENTITY_ORDER_REQUEST_ONLY_NO_CLEARANCE_OR_FILE_AUTHENTICITY_OR_EXPORT', 'cases': cases}
(raw / 'oracle-mutations.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result))
