#!/usr/bin/env python3
"""Exercise the actual GMP configure arguments for native/cross Apple and Linux."""
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class GmpBuildContract(unittest.TestCase):
    def arguments(self, apple, processor, target_arch='', cross=False):
        with tempfile.TemporaryDirectory(prefix='nptop-gmp-contract-') as directory:
            output = Path(directory) / 'arguments.txt'
            script = Path(directory) / 'probe.cmake'
            script.write_text(f'''cmake_minimum_required(VERSION 3.25)
set(APPLE {'TRUE' if apple else 'FALSE'})
set(MSVC FALSE)
set(CMAKE_SYSTEM_NAME {'Darwin' if apple else 'Linux'})
set(CMAKE_SYSTEM_PROCESSOR {processor})
set(CMAKE_OSX_ARCHITECTURES "{target_arch}")
set(IS_CROSS_COMPILE {'TRUE' if cross else 'FALSE'})
set(CMAKE_CROSSCOMPILING FALSE)
set(IN_GIT_REPO FALSE)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(DEP_OSX_TARGET 11.3)
set(DESTDIR "{directory}/prefix")
function(ExternalProject_Add name)
    file(WRITE "{output}" "${{ARGN}}")
endfunction()
include("{ROOT}/deps/GMP/GMP.cmake")
''')
            subprocess.run(['cmake', '-P', str(script)], check=True, capture_output=True, text=True)
            return output.read_text().split(';')

    def test_architecture_scope_and_pinned_source(self):
        scenarios = [(True, 'arm64', '', False, True),
                     (True, 'x86_64', 'arm64', True, True),
                     (True, 'arm64', 'x86_64', True, False),
                     (True, 'x86_64', '', False, False),
                     (False, 'aarch64', '', False, False)]
        for apple, processor, target, cross, disabled in scenarios:
            with self.subTest(apple=apple, processor=processor, target=target, cross=cross):
                args = self.arguments(apple, processor, target, cross)
                self.assertEqual('--disable-assembly' in args, disabled)
                self.assertIn('https://github.com/SoftFever/OrcaSlicer_deps/releases/download/gmp-6.2.1/gmp-6.2.1.tar.bz2', args)
                self.assertIn('SHA256=eae9326beb4158c386e39a356818031bd28f3124cf915f8c5b1dc4c7a36b4d7c', args)


if __name__ == '__main__':
    unittest.main()
