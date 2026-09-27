"""ORC-01: audit the real pinned checkout and deliberately inconsistent inputs."""
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BRIEF = ROOT / "docs/nonplanar"
spec = importlib.util.spec_from_file_location("source_audit", BRIEF / "scripts/audit_orca_checkout.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class CheckoutTests(unittest.TestCase):
    def setUp(self):
        self.lock = json.loads((BRIEF / "upstream/orca.lock.json").read_text())
        self.source_map = json.loads((BRIEF / "upstream/source_map.json").read_text())

    def test_ORC01_real_pinned_sources(self):
        report = audit.audit_checkout(ROOT, self.lock, self.source_map)
        self.assertEqual(report["status"], "PASS_SOURCE_INVENTORY_ONLY")
        self.assertEqual(report["baseline_commit"], self.lock["commit"])
        self.assertEqual(report["build_status"], "NOT_RUN")

    def test_ORC01_mismatched_lock_rejected(self):
        self.lock["commit"] = "0" * 40
        with self.assertRaisesRegex(ValueError, "different commits"):
            audit.audit_checkout(ROOT, self.lock, self.source_map)

    def test_ORC01_missing_commit_rejected(self):
        self.lock["commit"] = self.source_map["commit"] = "0" * 40
        with self.assertRaisesRegex(ValueError, "git rev-parse"):
            audit.audit_checkout(ROOT, self.lock, self.source_map)

    def test_ORC01_missing_history_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            subprocess.run(["git", "init", "-q", directory], check=True)
            with self.assertRaisesRegex(ValueError, "git rev-parse"):
                audit.audit_checkout(Path(directory), self.lock, self.source_map)


if __name__ == "__main__":
    unittest.main()
