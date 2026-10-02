import copy, hashlib, json, pathlib, subprocess, sys
root=pathlib.Path.cwd();raw=root/'build/nonplanar-evidence/B13-native-lineage';dest=raw/'oracle-mutations';dest.mkdir()
def sha(s):return hashlib.sha256(s.encode()).hexdigest()
def canonical(v):return json.dumps(v,sort_keys=True,separators=(',',':'),ensure_ascii=False,allow_nan=False)
base=json.loads((raw/'final-job-fixtures/candidate-report.json').read_text());cases=[]
def run(name,record,expected):
 path=dest/(name+'.json');path.write_text(json.dumps(record,indent=2)+'\n')
 log=dest/(name+'.txt');code=subprocess.run([sys.executable,'scripts/nonplanar/run_logged.py','--output',str(log),'--',sys.executable,'scripts/nonplanar/job_report_oracle.py','--input',str(path)]).returncode
 assert (code==0)==expected,(name,code)
 cases.append({'name':name,'exit_code':code,'expected':'PASS' if expected else 'REFUSE','record':str(path.relative_to(raw))})
for name,path in [('positive',raw/'final-job-fixtures/candidate-report.json'),('unknown-material',raw/'final-job-fixtures/candidate-report-invalid-material.json'),('failed-material',raw/'final-job-fixtures/candidate-report-failed-material.json'),('native',raw/'final-ctest-native/native-job-report.json'),('native-lineage',raw/'final-job-fixtures/native-lineage-report.json')]:run(name,json.loads(path.read_text()),True)
mutations={
 'registry-missing':lambda d:d['validation']['mandatory_check_ids'].pop(),
 'registry-duplicate':lambda d:d['validation']['mandatory_check_ids'].append('final_rates'),
 'registry-unknown':lambda d:d['validation']['mandatory_check_ids'].append('optional'),
 'checks-missing':lambda d:d['validation']['checks'].pop(),
 'checks-duplicate':lambda d:d['validation']['checks'].append(copy.deepcopy(d['validation']['checks'][0])),
 'checks-unknown':lambda d:d['validation']['checks'][0].update(id='optional'),
 'mandatory-false':lambda d:d['validation']['checks'][0].update(mandatory=False),
 'skipped-pass':lambda d:d['validation']['checks'][1].update(execution='SKIPPED'),
 'not-run-pass':lambda d:d['validation']['checks'][1].update(execution='NOT_RUN'),
 'unimplemented-pass':lambda d:d['validation']['checks'][0].update(status='PASS',execution='RUN'),
 'missing-reason':lambda d:d['validation']['checks'][0].update(reason=''),
 'allow':lambda d:d['validation'].update(export_decision='ALLOW'),
 'overall-pass':lambda d:d['validation'].update(overall_status='PASS'),
 'example':lambda d:d['validation'].update(document_example=True),
 'job-id':lambda d:d['validation'].update(job_id='88'),
 'revision':lambda d:d['validation'].update(job_revision=2),
 'attempt':lambda d:d.update(attempt=2),
 'job-fingerprint':lambda d:d.update(job_fingerprint='f'*64),
 'manifest-fingerprint':lambda d:d.update(manifest_sha256='f'*64),
 'records':lambda d:d['replay'].update(records=4),
 'evaluations':lambda d:d['replay'].update(evaluations=300),
 'registry-version':lambda d:d.update(registry_version=2),
 'schema':lambda d:d.update(schema=True),
 'scope':lambda d:d.update(scope='whole_job_verified'),
 'unknown-field':lambda d:d['validation'].update(approved=True),
 'replay-material-status':lambda d:d['replay'].update(material_status='FAIL'),
 'replay-journal-status':lambda d:d['validation']['checks'][9].update(status='FAIL'),
}
for name,mutate in mutations.items():
 record=copy.deepcopy(base);document=json.loads(record['canonical']);mutate(document);record['canonical']=canonical(document);record['sha256']=sha(record['canonical']);run(name,record,False)
for name in ['candidate-byte','manifest-rehashed','duplicate-json','nonfinite-json','overflow-json','boolean-revision','report-hash']:
 record=copy.deepcopy(base)
 if name=='candidate-byte':record['candidate_bytes']=record['candidate_bytes'].replace('X4.001234','X4.101234')
 elif name=='manifest-rehashed':
  manifest=json.loads(record['manifest']);manifest['candidate_size']+=1;record['manifest']=canonical(manifest);record['manifest_sha256']=sha(record['manifest'])
  document=json.loads(record['canonical']);document['manifest_sha256']=record['manifest_sha256'];record['canonical']=canonical(document);record['sha256']=sha(record['canonical'])
 elif name=='duplicate-json':record['canonical']=record['canonical'][:-1]+',"schema":1}';record['sha256']=sha(record['canonical'])
 elif name=='overflow-json':record['canonical']=record['canonical'].replace('"evaluations":299','"evaluations":1e999');record['sha256']=sha(record['canonical'])
 elif name=='boolean-revision':record['job_revision']=True
 elif name=='nonfinite-json':record['canonical']=record['canonical'].replace('"evaluations":299','"evaluations":NaN');record['sha256']=sha(record['canonical'])
 else:record['sha256']='f'*64
 run(name,record,False)
native_base=json.loads((raw/'final-job-fixtures/native-lineage-report.json').read_text())
def synchronize(record):
 n=record['native'];body=json.loads(n['body_canonical']);hatch=json.loads(n['hatch_canonical']);plan=json.loads(n['canonical'])
 for name in ['body','assembled','planned']:
  journal=n[name];digest=sha('nptop-material-ledger-v1\0'+journal['context'])
  for text in journal['records']:digest=sha('nptop-material-record-v1\0'+digest+text)
  journal['sha256']=digest
 body['body_journal']=n['body']['sha256'];n['body_canonical']=canonical(body);n['body_sha256']=sha(n['body_canonical'])
 hatch['body_lineage']=n['body_sha256'];n['hatch_canonical']=canonical(hatch);n['hatch_sha256']=sha(n['hatch_canonical'])
 plan.update(hatch_lineage=n['hatch_sha256'],assembled_journal=n['assembled']['sha256'],planned_journal=n['planned']['sha256'])
 n['canonical']=canonical(plan);n['sha256']=sha(n['canonical']);record['material_journal']=n['planned']['sha256']
 manifest=json.loads(record['manifest']);manifest.update(native_lineage=n['sha256'].encode().hex(),material_journal=record['material_journal'].encode().hex())
 record['manifest']=canonical(manifest);record['manifest_sha256']=sha(record['manifest'])
 document=json.loads(record['canonical']);document['manifest_sha256']=record['manifest_sha256'];record['canonical']=canonical(document);record['sha256']=sha(record['canonical'])
for name in ['source-bytes','source-units','body-owner','body-boolean-attempt','hatch-parent','candidate-parent','planned-position','planned-volume','planned-order','planned-speed','body-prefix','missing-native']:
 record=copy.deepcopy(native_base);n=record['native']
 if name.startswith('source-') or name.startswith('body-owner') or name=='body-boolean-attempt':
  body=json.loads(n['body_canonical'])
  if name=='source-bytes':body['source_sha256']='f'*64
  elif name=='source-units':body['request']['millimeters_declared']=False
  elif name=='body-boolean-attempt':body['attempt']=True
  else:body['job_fingerprint']='f'*64
  n['body_canonical']=canonical(body)
 elif name=='hatch-parent':
  hatch=json.loads(n['hatch_canonical']);hatch['body_lineage']='f'*64;n['hatch_canonical']=canonical(hatch)
 elif name=='candidate-parent':
  plan=json.loads(n['canonical']);plan['candidate_sha256']='f'*64;n['canonical']=canonical(plan)
 elif name.startswith('planned-'):
  index=next(i for i,text in enumerate(n['planned']['records']) if json.loads(text)['bead'] is not None)
  row=json.loads(n['planned']['records'][index])
  if name=='planned-position':row['motion'][4][0]='4035000000000000'
  elif name=='planned-volume':row['motion'][7][1]='3ff0000000000000'
  elif name=='planned-order':row['motion'][0]+=1
  else:row['motion'][5]='408f400000000000'
  n['planned']['records'][index]=canonical(row)
 elif name=='body-prefix':
  row=json.loads(n['assembled']['records'][0]);row['motion'][0]+=1;n['assembled']['records'][0]=canonical(row)
 if name!='hatch-parent':synchronize(record)
 else:
  n['hatch_sha256']=sha(n['hatch_canonical']);plan=json.loads(n['canonical']);plan['hatch_lineage']=n['hatch_sha256'];n['canonical']=canonical(plan);n['sha256']=sha(n['canonical'])
  manifest=json.loads(record['manifest']);manifest['native_lineage']=n['sha256'].encode().hex();record['manifest']=canonical(manifest);record['manifest_sha256']=sha(record['manifest'])
  document=json.loads(record['canonical']);document['manifest_sha256']=record['manifest_sha256'];record['canonical']=canonical(document);record['sha256']=sha(record['canonical'])
 if name=='missing-native':record.pop('native')
 run('native-'+name,record,False)
manifest={'status':'PASS','positives':sum(c['expected']=='PASS' for c in cases),'refusals':sum(c['expected']=='REFUSE' for c in cases),'scope':'REPORT_IDENTITY_FIXED_REGISTRY_ONLY_NOT_GEOMETRY_AUTHENTICITY_OR_EXPORT','cases':cases}
(dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest))
