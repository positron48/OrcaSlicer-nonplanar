from pathlib import Path
import argparse,json,hashlib
p=argparse.ArgumentParser();p.add_argument("--allow-deadline-counters",action="store_true");args=p.parse_args()
root=Path.cwd();raw=root/'build/nonplanar-evidence/B04-partition-order';old=root/'build/nonplanar-evidence/B12-supported-deposition'
counts={};outputs={};counter_drift={}
for stem in ['rate','material','cover','joined','nominal','support','travel','deposition','contact','polyline','supported']:
 current=raw/(stem+'-cli-final');previous=old/(stem+'-cli-final')
 report=json.loads((current/'manifest.json').read_text());assert report['status']=='PASS'
 counts[stem]=len(report['cases']);assert all(c['inputs_unchanged'] for c in report['cases'])
 a={str(p.relative_to(previous)):hashlib.sha256(p.read_bytes()).hexdigest() for p in previous.rglob('stdout.json')}
 b={str(p.relative_to(current)):hashlib.sha256(p.read_bytes()).hexdigest() for p in current.rglob('stdout.json')}
 assert set(a)==set(b),stem
 different=[n for n in a if a[n]!=b[n]]
 if different:
  assert args.allow_deadline_counters and stem=='nominal' and sorted(different)==['pause/stdout.json','pressure/stdout.json','turn/stdout.json'],(stem,different)
  for name in different:
   x=json.loads((previous/name).read_text());y=json.loads((current/name).read_text())
   changed={k:[x[k],y[k]] for k in x if x[k]!=y[k]}
   assert set(x)==set(y) and set(changed)=={'work','cells'},(name,changed)
   assert x['component_status']==y['component_status']=='UNKNOWN' and x['reason']==y['reason']=='CANCELLED'
   assert x['export_allowed'] is y['export_allowed'] is False and x['cover_leaves']==y['cover_leaves']==0
   assert 0<x['work']<=2000000 and 0<y['work']<=2000000 and 0<x['cells']<=100000 and 0<y['cells']<=100000
   counter_drift[name]={'parent_sha256':a[name],'current_sha256':b[name],'changed_fields':changed,'status':'UNCHANGED_UNKNOWN_ABSOLUTE_1S_DEADLINE_WORK_CELL_COUNTS_ONLY'}
 outputs[stem]=len(b)-len(different)
assert sum(counts.values())==437 and sum(outputs.values())==434
supported=json.loads((raw/'supported-cli-final/manifest.json').read_text());assert supported['status']=='PASS' and len(supported['cases'])==105
assert all(c['inputs_unchanged'] for c in supported['cases'])
for stem,cells,work in [('travel',133,419381),('deposition',300,708889),('contact',28,270219),('polyline',28,270344)]:
 report=json.loads((raw/('native-'+stem+'-cli-final.txt')).read_text())
 assert report==json.loads((old/('native-'+stem+'-cli-final.txt')).read_text()),stem
 assert report['component_status']=='PASS' and report['job_status']=='UNKNOWN' and report['export_allowed'] is False
 assert report['cells']==cells and report['work']==work
report=json.loads((raw/'native-supported-cli-final.txt').read_text())
assert report['component_status']=='PASS' and report['job_status']=='UNKNOWN' and report['export_allowed'] is False
assert report['cells']==48 and report['geometry_cells']==28 and report['geometry_leaves']==28 and report['work']==598131
assert report['support_runs']==1 and report['underlying_completed_records']==2198
assert report['geometry_witness'] is None and report['support_witness'] is None
result={'status':'PASS434_EXACT_REPORTS_THREE_DEADLINE_COUNTERS_AND_FRESH_NATIVE_PROOFS','counts':counts,'exact_outputs':outputs,'new_cases':len(supported['cases']),'deadline_counter_differences':counter_drift,'native_supported':report}
(raw/'cli-regressions.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'status':result['status'],'previous_cases':sum(counts.values()),'new_cases':105,'native_work':report['work']}))
