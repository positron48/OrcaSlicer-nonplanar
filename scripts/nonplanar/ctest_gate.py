#!/usr/bin/env python3
"""Bootstrap test gate: disabled, empty, unbuilt or skipped tests cannot pass."""
import argparse
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET


def run(build_dir, output_dir, configuration="Release", regex=None):
    cache = build_dir / "CMakeCache.txt"
    if not cache.is_file() or "BUILD_TESTS:BOOL=ON" not in cache.read_text().splitlines():
        raise ValueError("BUILD_TESTS must be ON in the actual build cache")
    output_dir.mkdir(parents=True, exist_ok=True)
    command = ["ctest", "--test-dir", str(build_dir), "-C", configuration]
    if regex:
        command += ["-R", regex]
    discovery = subprocess.run(command + ["--show-only=json-v1"],
                               capture_output=True, text=True, check=True)
    (output_dir / "discovery.json").write_text(discovery.stdout)
    tests = json.loads(discovery.stdout)["tests"]
    if not tests or any(not test.get("command") for test in tests):
        raise ValueError("CTest selection is empty or contains an unbuilt executable")
    junit = (output_dir / "results.xml").resolve()
    result = subprocess.run(command + ["--no-tests=error", "--output-on-failure",
                                       "--output-junit", str(junit)], check=False)
    if result.returncode:
        raise ValueError(f"CTest failed with exit code {result.returncode}")
    cases = ET.parse(junit).getroot().findall(".//testcase")
    if len(cases) != len(tests) or any(case.get("status") != "run" or
            any(case.find(tag) is not None for tag in ("skipped", "failure", "error"))
            for case in cases):
        raise ValueError("CTest did not execute every selected test successfully")
    return {"tests_discovered": len(tests), "tests_executed": len(cases),
            "test_names": [test["name"] for test in tests], "status": "PASS"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--configuration", default="Release")
    parser.add_argument("--regex")
    args = parser.parse_args()
    try:
        print(json.dumps(run(args.build_dir, args.output_dir, args.configuration, args.regex), indent=2))
    except (ValueError, OSError, subprocess.CalledProcessError, ET.ParseError, KeyError) as exc:
        parser.exit(2, f"CTest gate failed: {exc}\n")
