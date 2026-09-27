"""Tests of bundle helpers only. These do not exercise Orca or a G-code verifier."""
from __future__ import annotations
from collections import Counter
import copy
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest

import generate_fixtures as gf
import validate_package as vp
import audit_orca_checkout as audit

ROOT = Path(__file__).resolve().parents[1]

class FixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = gf.load_catalog(ROOT / "fixtures/catalog.json")
        cls.models = {m["id"]: m for m in cls.catalog["models"]}

    def test_all_models_are_closed_two_manifold(self):
        for m in self.models.values():
            with self.subTest(model=m["id"]):
                tris = gf.read_stl(gf.encode_stl(m["id"], gf.mesh(m)))
                edges = Counter()
                for a,b,c in tris:
                    for p,q in ((a,b),(b,c),(c,a)):
                        edges[tuple(sorted((p,q)))] += 1
                self.assertTrue(all(count == 2 for count in edges.values()))

    def test_all_edge_orientations_are_opposite(self):
        for m in self.models.values():
            tris = gf.mesh(m)
            directions = Counter((p,q) for a,b,c in tris for p,q in ((a,b),(b,c),(c,a)))
            with self.subTest(model=m["id"]):
                self.assertTrue(all(directions[(q,p)] == n for (p,q),n in directions.items()))

    def test_all_normals_finite_and_unit(self):
        for m in self.models.values():
            for tri in gf.mesh(m):
                self.assertAlmostEqual(sum(v*v for v in gf.normal(tri)), 1.0, places=10)

    def test_all_volumes_positive(self):
        for m in self.models.values():
            with self.subTest(model=m["id"]):
                payload = gf.encode_stl(m["id"], gf.mesh(m))
                self.assertGreater(gf.signed_volume(gf.read_stl(payload)), 0)

    def test_flat_exact_analytic_volume(self):
        self.assertAlmostEqual(gf.signed_volume(gf.mesh(self.models['flat_block'])), 30*20*4, places=8)

    def test_wedge_analytic_volume(self):
        for name in ('wedge_5deg','wedge_10deg','wedge_30deg'):
            m=self.models[name]; lx,ly=m['extent_xy_mm']; p=m['parameters']
            expected=lx*ly*(p['height_mm']+math.tan(math.radians(p['angle_deg']))*lx/2)
            self.assertAlmostEqual(gf.signed_volume(gf.mesh(m)),expected,places=7)

    def test_binary_length_and_roundtrip(self):
        for m in self.models.values():
            tris=gf.mesh(m); payload=gf.encode_stl(m['id'],tris)
            self.assertEqual(len(payload),84+len(tris)*50)
            self.assertEqual(len(gf.read_stl(payload)),len(tris))

    def test_generation_is_deterministic(self):
        m=self.models['shallow_sphere']
        self.assertEqual(gf.encode_stl(m['id'],gf.mesh(m)),gf.encode_stl(m['id'],gf.mesh(m)))

    def test_truncated_stl_rejected(self):
        with self.assertRaises(ValueError): gf.read_stl(b'bad')
        payload=gf.encode_stl('flat_block',gf.mesh(self.models['flat_block']))
        with self.assertRaises(ValueError): gf.read_stl(payload[:-1])

    def test_bad_grid_rejected(self):
        m=copy.deepcopy(self.models['flat_block']);m['grid_xy']=[0,3]
        with self.assertRaises(ValueError):gf.mesh(m)

    def test_bad_domain_rejected(self):
        m=copy.deepcopy(self.models['shallow_sphere']);m['parameters']['radius_mm']=1
        with self.assertRaises(ValueError):gf.height(m,10,10)

    def _catalog_rejects(self,data):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'catalog.json';p.write_text(json.dumps(data))
            with self.assertRaises(ValueError):gf.load_catalog(p)

    def test_unsafe_and_duplicate_ids_rejected(self):
        data=copy.deepcopy(self.catalog);data['models'][0]['id']='../../bad';self._catalog_rejects(data)
        data=copy.deepcopy(self.catalog);data['models'].append(data['models'][0]);self._catalog_rejects(data)

    def test_unknown_shape_rejected(self):
        data=copy.deepcopy(self.catalog);data['models'][0]['shape']='execute_python';self._catalog_rejects(data)

    def test_regenerated_hashes_match_distributed_models(self):
        mf=json.loads((ROOT/'fixtures/models/manifest.json').read_text())
        for entry in mf['models']:
            m=self.models[entry['id']]
            self.assertEqual(hashlib.sha256(gf.encode_stl(m['id'],gf.mesh(m))).hexdigest(),entry['sha256'])

class DocumentationHelperTests(unittest.TestCase):
    def test_valid_dag(self):
        vp.check_dag([{'id':'A','dependencies':[]},{'id':'B','dependencies':['A']}])

    def test_cycle_rejected(self):
        with self.assertRaises(ValueError):vp.check_dag([{'id':'A','dependencies':['B']},{'id':'B','dependencies':['A']}])

    def test_unknown_dependency_rejected(self):
        with self.assertRaises(ValueError):vp.check_dag([{'id':'A','dependencies':['MISSING']}])

    def test_duplicate_ids_rejected(self):
        with self.assertRaises(ValueError):vp.unique_ids([{'id':'A'},{'id':'A'}],'test')

    def test_checksums_detect_tampering(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d); p=root/'a.txt';p.write_bytes(b'original')
            (root/'SHA256SUMS.txt').write_text(hashlib.sha256(p.read_bytes()).hexdigest()+'  a.txt\n')
            self.assertEqual(vp.verify_checksums(root),1)
            p.write_bytes(b'changed')
            with self.assertRaises(ValueError):vp.verify_checksums(root)

    def test_checksum_path_escape_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'SHA256SUMS.txt').write_text('0'*64+'  ../outside\n')
            with self.assertRaises(ValueError):vp.verify_checksums(root)

    def test_broken_document_link_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'README.md').write_text('[bad](missing.md)')
            with self.assertRaises(ValueError):vp.check_links(root)

@unittest.skipUnless(shutil.which('git'), 'Git not installed; checkout helper tests NOT_RUN')
class SourceAuditTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        def cmd(*args):
            subprocess.run(['git','-C',str(self.root),*args],check=True,capture_output=True)
        cmd('init','-q');(self.root/'source.cpp').write_text('marker_xyz\n')
        cmd('add','source.cpp');cmd('-c','user.name=Helper Test','-c','user.email=helper@example.invalid','commit','-qm','fixture')
        self.sha=audit.git(self.root,'rev-parse','HEAD')
        self.lock={'commit':self.sha};self.sm={'commit':self.sha,'files':[{'path':'source.cpp','markers':['marker_xyz']}]}

    def tearDown(self):self.tmp.cleanup()

    def test_local_source_inventory(self):
        result=audit.audit_checkout(self.root,self.lock,self.sm)
        self.assertEqual(result['status'],'PASS_SOURCE_INVENTORY_ONLY')
        self.assertEqual(result['build_status'],'NOT_RUN')

    def test_changed_marker_rejected(self):
        (self.root/'source.cpp').write_text('different marker')
        with self.assertRaises(ValueError):audit.audit_checkout(self.root,self.lock,self.sm)

    def test_mismatched_commit_rejected(self):
        self.sm['commit']='0'*40
        with self.assertRaises(ValueError):audit.audit_checkout(self.root,self.lock,self.sm)

    def test_unsafe_source_path_rejected(self):
        self.sm['files'][0]['path']='../source.cpp'
        with self.assertRaises(ValueError):audit.audit_checkout(self.root,self.lock,self.sm)

if __name__ == '__main__':unittest.main()
