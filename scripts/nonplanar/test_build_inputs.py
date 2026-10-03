#!/usr/bin/env python3
"""Exercise content-based build inventory updates in isolated CMake projects."""
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from build_inputs_oracle import verify, verify_binding

ROOT = Path(__file__).resolve().parents[2]


class BuildInputsTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="nptop build inputs ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "source"
        self.output = Path(self.temporary.name) / "output"
        for directory in ("cmake/modules", "src", "resources", "deps_src", "docs/nonplanar/upstream"):
            (self.root / directory).mkdir(parents=True)
        for name in ("GenerateNonplanarBuildInputs.cmake", "NonplanarBuildInputs.cmake"):
            shutil.copyfile(ROOT / "cmake/modules" / name, self.root / "cmake/modules" / name)
        (self.root / "version.inc").write_text("fixture version\n")
        (self.root / "docs/nonplanar/upstream/orca.lock.json").write_text('{"fixture":true}\n')
        (self.root / "resources/голова space.json").write_text('{"nozzle":0.4}\n')
        self.output.mkdir()
        self.inputs = ["CMakeLists.txt", "resources/голова space.json", "version.inc"]
        self.commands = []
        for config in ("Release", "Debug"):
            (self.output / config).mkdir()
            (self.output / config / "context.cmake").write_text(
                f'set(configuration "{config}")\nset(CMAKE_CXX_COMPILER_ID "fixture")\n'
                'set(CMAKE_CXX_FLAGS [=[flag \\"quoted\\";semi]=])\n')

    def run_command(self, command, success=True):
        result = subprocess.run(command, capture_output=True, text=True)
        self.commands.append((command, result.stdout, result.stderr))
        if success:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
        return result

    def generate(self, success=True):
        (self.root / "CMakeLists.txt").write_text("# standalone generator fixture\n")
        (self.output / "files.txt").write_text("\n".join(self.inputs) + "\n")
        return self.run_command(["cmake", f"-DNPTOP_SOURCE_ROOT={self.root}",
            f"-DNPTOP_OUTPUT={self.output}", "-DNPTOP_CONFIGURATIONS=Release|Debug", "-P",
            str(self.root / "cmake/modules/GenerateNonplanarBuildInputs.cmake")], success)

    def test_content_not_timestamp_and_unchanged_header(self):
        self.generate()
        header = self.output / "Release/NonplanarBuildInputsData.hpp"
        first, stamp = header.read_bytes(), header.stat().st_mtime_ns
        self.generate()
        self.assertEqual(header.stat().st_mtime_ns, stamp)
        self.assertEqual(header.read_bytes(), first)
        resource = self.root / self.inputs[1]
        original_stat = resource.stat()
        resource.write_text('{"nozzle":0.6}\n')
        os.utime(resource, ns=(original_stat.st_atime_ns, original_stat.st_mtime_ns))
        self.generate()
        self.assertNotEqual(header.read_bytes(), first)
        value = json.loads((self.output / "Release/inventory.json").read_bytes())
        self.assertEqual(dict(value["files"])[self.inputs[1]], hashlib.sha256(resource.read_bytes()).hexdigest())
        self.assertNotEqual((self.output / "Release/inventory.json").read_bytes(),
                            (self.output / "Debug/inventory.json").read_bytes())

    def test_missing_duplicate_and_unsorted_inputs_fail(self):
        for inputs in ([], ["missing.cpp"], ["../version.inc"], ["version.inc", "version.inc"], ["version.inc", "CMakeLists.txt"]):
            with self.subTest(inputs=inputs):
                self.inputs = inputs
                self.generate(False)

    def test_invalidation_refuses_outputs_outside_the_build_tree(self):
        sentinel = self.root / "outside-build.txt"
        sentinel.write_text("preserved")
        for config in ("Release", "Debug"):
            context = self.output / config / "context.cmake"
            with context.open("a") as stream:
                stream.write('set(CMAKE_GENERATOR "Unix Makefiles")\n'
                             f'set(inventory_build_root [=[{self.output}]=])\n')
            (self.output / config / "artifacts.cmake").write_text(f'set(inventory_artifacts [=[{sentinel}]=])\n')
        self.generate(False)
        self.assertEqual(sentinel.read_text(), "preserved")

    def test_native_generator_tracks_resources_added_and_removed(self):
        self.exercise_native_generator(False)

    def test_static_inventory_producer_relinks_transitive_consumers_only(self):
        self.exercise_native_generator(True)

    def test_existing_nesting_static_cycle_remains_buildable_and_fresh(self):
        self.exercise_native_generator("cycle")

    def exercise_native_generator(self, static):
        source = self.root / "src/Nonplanar/BuildInputs.cpp"
        source.parent.mkdir()
        if static:
            source.write_text('#include "NonplanarBuildInputsData.hpp"\n'
                              'const char *inventory() { return Slic3r::nptop::detail::build_inputs_sha256; }\n')
            (self.root / "src/main.cpp").write_text('#include <iostream>\nconst char *inventory();\nint main() {std::cout << inventory();}\n')
            (self.root / "src/wrapper.cpp").write_text('void wrapper() {}\n')
            (self.root / "src/unrelated.cpp").write_text('int main() {}\n')
            targets = ('add_library(core STATIC src/Nonplanar/BuildInputs.cpp)\n'
                       'add_library(wrapper STATIC src/wrapper.cpp)\ntarget_link_libraries(wrapper PUBLIC core)\n'
                       'add_executable(fixture src/main.cpp)\ntarget_link_libraries(fixture PRIVATE wrapper)\n'
                       'add_executable(unrelated src/unrelated.cpp)\n')
            if static == "cycle":
                targets = targets.replace("wrapper", "libnest2d").replace("src/libnest2d.cpp", "src/wrapper.cpp")
                targets += 'target_link_libraries(core PRIVATE libnest2d)\n'
            producer = 'core'
        else:
            source.write_text('#include "NonplanarBuildInputsData.hpp"\n#include <iostream>\n'
                              'int main() { std::cout << Slic3r::nptop::detail::build_inputs_sha256; }\n')
            targets = 'add_executable(fixture src/Nonplanar/BuildInputs.cpp)\n'
            producer = 'fixture'
        (self.root / "CMakeLists.txt").write_text('cmake_minimum_required(VERSION 3.13)\nproject(Fixture LANGUAGES CXX)\n'
            'set(CMAKE_CXX_STANDARD 17)\nset(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}")\n'
            'list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")\n'
            + targets + 'include(NonplanarBuildInputs)\n'
            + f'nonplanar_build_inputs({producer} src/Nonplanar/BuildInputs.cpp)\n'
            + 'nonplanar_build_input_artifacts("${CMAKE_SOURCE_DIR}")\n')
        build = self.output / "build"
        self.run_command(["cmake", "-S", str(self.root), "-B", str(build), "-DCMAKE_BUILD_TYPE=Release"])
        command = ["cmake", "--build", str(build), "--target", "fixture", "--config", "Release"]
        self.run_command(command)
        if static:
            self.run_command(["cmake", "--build", str(build), "--target", "unrelated", "--config", "Release"])
            unrelated = build / ("unrelated.exe" if os.name == "nt" else "unrelated")
            unrelated_stamp = unrelated.stat().st_mtime_ns
        inventory = build / "nonplanar-build-inputs/Release/inventory.json"
        binary = build / ("fixture.exe" if os.name == "nt" else "fixture")
        def check_binary():
            actual = self.run_command([str(binary)]).stdout
            self.assertEqual(actual, hashlib.sha256(inventory.read_bytes()).hexdigest(),
                f"header mtime={inventory.with_name('NonplanarBuildInputsData.hpp').stat().st_mtime_ns}, "
                f"binary mtime={binary.stat().st_mtime_ns}\n" + repr(self.commands[-3:]))
        check_binary()
        self.assertEqual(verify(inventory, self.root)["status"], "PASS")
        header = inventory.with_name("NonplanarBuildInputsData.hpp")
        before, stamp = header.read_bytes(), header.stat().st_mtime_ns
        self.run_command(command)
        self.assertEqual(header.stat().st_mtime_ns, stamp)
        resource = self.root / "resources/голова space.json"
        original_stat = resource.stat()
        resource.write_text('{"nozzle":0.6}\n')
        os.utime(resource, ns=(original_stat.st_atime_ns, original_stat.st_mtime_ns))
        self.run_command(command)
        check_binary()
        self.assertNotEqual(header.read_bytes(), before)
        before = header.read_bytes()
        resource = self.root / "resources/added.json"
        resource.write_text("added\n")
        self.run_command(command)
        self.assertNotEqual(header.read_bytes(), before)
        self.assertEqual(verify(inventory, self.root)["status"], "PASS")
        check_binary()
        if static:
            self.assertEqual(unrelated.stat().st_mtime_ns, unrelated_stamp)
        resource.unlink()
        self.run_command(command)
        self.assertEqual(header.read_bytes(), before)
        self.assertEqual(verify(inventory, self.root)["status"], "PASS")
        check_binary()

    def test_large_utf8_inventory_preserves_exact_bytes_across_literal_chunks(self):
        for i in range(300):
            name = f"resources/голова {i:03}.json"
            (self.root / name).write_text(str(i))
            self.inputs.append(name)
        self.inputs.sort()
        self.generate()
        header = (self.output / "Release/NonplanarBuildInputsData.hpp").read_text()
        parts = [match[2] for match in re.finditer(r'R"([a-f0-9]{12})\((.*?)\)\1"', header, re.DOTALL)]
        self.assertGreater(len(parts), 1)
        self.assertTrue(all(len(part.encode()) <= 8000 for part in parts))
        self.assertEqual("".join(parts).encode(), (self.output / "Release/inventory.json").read_bytes())

    def test_independent_job_binding_rejects_declared_or_changed_software(self):
        self.generate()
        inventory = self.output / "Release/inventory.json"
        row = [7, "compiled-build-inputs-v1".encode().hex(),
               hashlib.sha256(inventory.read_bytes()).hexdigest().encode().hex(), inventory.stat().st_size]
        report = self.output / "report.json"
        def save(resource):
            canonical = json.dumps({"resources": [resource]}, separators=(",", ":"))
            report.write_text(json.dumps({"job_canonical": canonical,
                "job_fingerprint": hashlib.sha256(canonical.encode()).hexdigest()}))
        save(row)
        self.assertEqual(verify_binding(inventory, report)["status"], "PASS")
        for index, replacement in ((1, "declared-software".encode().hex()),
                                   (2, ("0" * 64).encode().hex()), (3, row[3] + 1)):
            changed = row.copy()
            changed[index] = replacement
            save(changed)
            with self.assertRaises(AssertionError):
                verify_binding(inventory, report)


if __name__ == "__main__":
    unittest.main()
