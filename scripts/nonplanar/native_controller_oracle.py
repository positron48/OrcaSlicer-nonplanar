#!/usr/bin/env python3
"""Independent initial request/job/body/hatch/report identity linkage only."""
import argparse
import copy
import hashlib
import json

from job_report_oracle import canonical, parse, require, verify as verify_report
from native_job_inputs_oracle import verify_inputs


def sha(text):
    return hashlib.sha256(text.encode('utf-8')).hexdigest()


def verify_request(record):
    text = record['analysis_request_canonical']
    digest = record['analysis_request_sha256']
    request = parse(text)
    require(canonical(request) == text and sha(text) == digest, 'Owned request canonical/hash')
    later = 'later_paths' in request
    require(set(request) == {'schema', 'contour', 'fill_region', 'footprint', 'hatches', 'inputs',
                             'millimeters_declared', 'passes', 'patch', 'reservation', 'support_plane'} |
            ({'later_paths'} if later else set()) and
            type(request['schema']) is int and request['schema'] == (2 if later else 1), 'Owned request registry')
    if later:
        require(0 < len(request['later_paths']) <= 4096 and
                request['later_paths'] == parse(record['native']['later_canonical'])['requests'], 'Owned ordered later requests')
    job = parse(record['job_canonical'])
    row = [0, 'native-analysis-request-v1'.encode().hex(), digest.encode().hex(), len(text.encode())]
    require(job['resources'].count(row) == 1, 'Whole request belongs to the actual job')
    require(request['inputs'] == [[r['kind'], r['name'].encode().hex(), r['sha256'].encode().hex()]
                                 for r in record['native_inputs']], 'All actual typed role inputs')
    body = parse(record['native']['body_canonical'])['request']
    hatches = parse(record['native']['hatch_canonical'])['request']
    require(request['millimeters_declared'] == body['millimeters_declared'] and
            request['reservation'] == body['reservation'], 'Actual reservation/units')
    for key in ['footprint', 'passes', 'patch', 'support_plane']:
        require(request[key] == hatches[key], 'Actual native hatch request: ' + key)
    require(request['hatches'] == hatches['hatch'], 'Actual native hatch policy')
    # Explicit known fixture declarations, independent from controller output.
    from native_job_inputs_oracle import bits, box
    require(request['contour'] == [bits(.4), 0, False, bits(.001)], 'Original contour choice')
    rectangle = request['footprint']
    require(request['fill_region'] == [[rectangle[0], rectangle[1], bits(4.0)],
                                       [rectangle[2], rectangle[3], bits(4.7)]], 'Original fill region')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True)
    args = parser.parse_args()
    with open(args.input) as stream:
        record = parse(stream.read())
    verify_report(record)
    verify_inputs(record)
    verify_request(record)
    refused = 0
    for key in ['reservation', 'footprint', 'passes', 'hatches', 'contour', 'fill_region', 'inputs']:
        changed = copy.deepcopy(record)
        request = parse(changed['analysis_request_canonical'])
        request[key] = []
        changed['analysis_request_canonical'] = canonical(request)
        changed['analysis_request_sha256'] = sha(changed['analysis_request_canonical'])
        try:
            verify_request(changed)
        except ValueError:
            refused += 1
        else:
            raise ValueError('Changed initial request accepted')
    print(json.dumps({'status': 'PASS', 'mutation_refusals': refused,
                      'scope': 'OWNED_REQUEST_JOB_ACTUAL_NATIVE_STAGE_AND_REPORT_IDENTITY_ONLY', 'export': 'BLOCK'}))


if __name__ == '__main__':
    main()
