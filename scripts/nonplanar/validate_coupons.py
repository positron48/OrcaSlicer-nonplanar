#!/usr/bin/env python3
"""Independent structural/analytic checks of the prepared binary STL coupons."""
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct

import sys

def check(condition):
    if not condition:
        raise ValueError("coupon geometry validation failed")

root = Path(__file__).resolve().parents[2] / "docs/nonplanar-dev/coupons/models"
if len(sys.argv) == 2:
    root = Path(sys.argv[1])
manifest = json.loads((root / "manifest.json").read_text())
check({r["id"] for r in manifest["models"]} == {"coupon_flat_reference","coupon_wedge_5deg"} and len(manifest["models"]) == 2)
results = []
for record in manifest["models"]:
    payload = (root / record["file"]).read_bytes()
    count, = struct.unpack_from("<I", payload, 80)
    check(len(payload) == 84+50*count)
    check(hashlib.sha256(payload).hexdigest() == record["sha256"])
    edges, oriented = Counter(), Counter()
    terms = []
    for i in range(count):
        data = struct.unpack_from("<12fH",payload,84+50*i)
        check(all(math.isfinite(v) for v in data[:12]))
        a,b,c = (tuple(data[j:j+3]) for j in (3,6,9))
        u,v = [b[k]-a[k] for k in range(3)],[c[k]-a[k] for k in range(3)]
        normal = (u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
        check(sum(n*n for n in normal)>0)
        for p,q in ((a,b),(b,c),(c,a)):
            edges[tuple(sorted((p,q)))] += 1
            oriented[(p,q)] += 1
        terms.append((a[0]*(b[1]*c[2]-b[2]*c[1])+a[1]*(b[2]*c[0]-b[0]*c[2])+a[2]*(b[0]*c[1]-b[1]*c[0]))/6)
    check(all(n==2 for n in edges.values()))
    check(all(oriented[(q,p)]==n for (p,q),n in oriented.items()))
    volume=math.fsum(terms)
    expected=400 if record['id']=='coupon_flat_reference' else 200*(2+10*math.tan(math.pi/36))
    check(abs(volume-expected)<1e-4)
    results.append({'id':record['id'],'triangles':count,'expected_volume_mm3':expected,'actual_volume_mm3':volume})
print(json.dumps({'status':'GEOMETRY_CHECKS_PASS_PHYSICAL_NOT_RUN','models':results},indent=2))
