import tempfile
import unittest
from pathlib import Path
from publish_ci_failure import annotation, diagnostics


class FailureDiagnostics(unittest.TestCase):
    def test_workflow_commands_are_escaped_data(self):
        value = annotation("100%\r\n::notice::not a command")
        self.assertEqual(value.count("\n"), 0)
        self.assertIn("100%25%0D%0A::notice::not a command", value)

    def test_missing_report_is_explicit(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertIn("before usable CTest diagnostics", diagnostics(Path(directory))[0])

    def test_gate_error_before_junit_is_retained(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "tests.log").write_text("unrelated\nCTest gate failed: Missing requested CTest target executable\n")
            result = diagnostics(root)
            self.assertEqual(len(result), 1)
            self.assertIn("Missing requested CTest target executable", result[0])

    def test_failed_skipped_and_not_run_native_cases_are_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "ctest").mkdir()
            (root / "ctest/results.xml").write_text('''<testsuite>
              <testcase name="passed" status="run"/>
              <testcase name="native failure" status="fail"><failure>Expected 12</failure><system-out>Observed 13</system-out></testcase>
              <testcase name="skipped" status="run"><skipped>disabled</skipped></testcase>
              <testcase name="unbuilt" status="notrun"/>
            </testsuite>''')
            result = diagnostics(root)
            self.assertEqual(len(result), 3)
            self.assertIn("native failure%0AExpected 12%0AObserved 13", result[0])
            self.assertIn("disabled", result[1])
            self.assertIn("unbuilt", result[2])

    def test_large_outputs_are_bounded_and_excess_cases_counted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "ctest").mkdir()
            case = '<testcase name="bad" status="fail"><failure>' + 'x' * 10000 + '</failure></testcase>'
            (root / "ctest/results.xml").write_text('<testsuite>' + case * 8 + '</testsuite>')
            result = diagnostics(root)
            self.assertEqual(len(result), 6)
            self.assertTrue(all(len(line) < 2200 for line in result))
            self.assertIn("3 additional", result[-1])

    def test_corrupt_junit_keeps_original_gate_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "ctest").mkdir()
            (root / "tests.log").write_text("CTest gate failed: CTest failed with exit code 8\n")
            (root / "ctest/results.xml").write_text("<unfinished>")
            result = diagnostics(root)
            self.assertIn("exit code 8", result[0])
            self.assertIn("Cannot read native test report", result[1])


if __name__ == "__main__":
    unittest.main()
