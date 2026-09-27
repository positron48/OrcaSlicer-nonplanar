"""ORC-31 gate tests using real CMake/CTest, not slicer test evidence."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from ctest_gate import run


class CTestGateTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.build = self.root / "build"

    def configure(self, tests, enabled=True):
        (self.root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.16)\nproject(GateFixture NONE)\n'
            'option(BUILD_TESTS "Gate fixture" ON)\nenable_testing()\n' + tests)
        subprocess.run(["cmake", "-S", str(self.root), "-B", str(self.build),
                        f"-DBUILD_TESTS={'ON' if enabled else 'OFF'}"],
                       check=True, capture_output=True)

    def gate(self, regex=None):
        return run(self.build, self.root / "evidence", regex=regex)

    def test_ORC31_positive_executes_test(self):
        self.configure('add_test(NAME compare COMMAND ${CMAKE_COMMAND} -E compare_files '
                       '${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt)\n')
        self.assertEqual(self.gate()["tests_executed"], 1)

    def test_ORC31_empty_rejected(self):
        self.configure("")
        with self.assertRaisesRegex(ValueError, "empty"):
            self.gate()

    def test_ORC31_disabled_rejected(self):
        self.configure("", enabled=False)
        with self.assertRaisesRegex(ValueError, "BUILD_TESTS"):
            self.gate()

    def test_ORC31_nonmatching_selection_rejected(self):
        self.configure('add_test(NAME one COMMAND ${CMAKE_COMMAND} -E true)\n')
        with self.assertRaisesRegex(ValueError, "empty"):
            self.gate("does_not_exist")

    def test_ORC31_failure_rejected(self):
        self.configure('add_test(NAME failure COMMAND ${CMAKE_COMMAND} -E false)\n')
        with self.assertRaisesRegex(ValueError, "CTest failed"):
            self.gate()

    def test_ORC31_unbuilt_rejected(self):
        self.configure('add_test(NAME unbuilt COMMAND ${CMAKE_CURRENT_BINARY_DIR}/missing_executable)\n')
        with self.assertRaisesRegex(ValueError, "unbuilt"):
            self.gate()

    def test_ORC31_skip_rejected(self):
        self.configure('add_test(NAME skip COMMAND ${CMAKE_COMMAND} -E false)\n'
                       'set_tests_properties(skip PROPERTIES SKIP_RETURN_CODE 1)\n')
        with self.assertRaisesRegex(ValueError, "every selected test"):
            self.gate()


if __name__ == "__main__":
    unittest.main()
