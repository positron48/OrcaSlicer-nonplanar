from pathlib import Path
import sys
import tempfile
import unittest
from benchmark_native import measure


class BenchmarkEvidenceTest(unittest.TestCase):
    def run_fixture(self, cases):
        with tempfile.TemporaryDirectory() as root:
            directory = Path(root)
            runner = directory / "fixture.py"
            xml = '<testsuites><testsuite tests="2">'+cases+'</testsuite></testsuites>'
            runner.write_text("import pathlib,sys\npathlib.Path(sys.argv[-1]).write_text("+repr(xml)+")\n")
            return measure([sys.executable, str(runner)], directory, ["parent"])

    def test_dynamic_sections_are_not_extra_top_level_tests(self):
        result = self.run_fixture('<testcase name="parent" status="run"/>'
                                  '<testcase name="parent/section" status="run"/>')
        self.assertTrue(result["valid"])
        self.assertEqual(result["cases_executed"], 1)
        self.assertEqual(result["junit_case_entries"], 2)
        self.assertGreater(result["peak_rss_bytes"], 0)
        self.assertGreater(result["wall_seconds"], 0)

    def test_missing_skipped_failed_or_error_case_cannot_be_benchmark_success(self):
        for cases in ('', '<testcase name="other" status="run"/>',
                      '<testcase name="parent" status="run"><skipped/></testcase>',
                      '<testcase name="parent" status="run"><failure/></testcase>',
                      '<testcase name="parent" status="run"><error/></testcase>'):
            with self.subTest(cases=cases), self.assertRaises(RuntimeError):
                self.run_fixture(cases)
