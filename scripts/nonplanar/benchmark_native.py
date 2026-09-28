#!/usr/bin/env python3
"""Measure bounded native Gate A test workloads, never the full SYS-08 pipeline."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import threading
import time
import xml.etree.ElementTree as ET


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def measure(command, directory, expected_names):
    xml = directory / "results.xml"
    argv = command + ["--rng-seed", "4708", "--order", "rand", "--warn", "NoAssertions",
                      "--reporter", "junit", "--out", str(xml)]
    timed_out = threading.Event()
    start = time.perf_counter()
    with (directory / "stdout.txt").open("xb") as out, (directory / "stderr.txt").open("xb") as err:
        process = subprocess.Popen(argv, stdout=out, stderr=err)

        def timeout():
            timed_out.set()
            process.kill()

        timer = threading.Timer(60, timeout)
        timer.start()
        try:
            _, status, usage = os.wait4(process.pid, 0)
            process.returncode = os.waitstatus_to_exitcode(status)
        except BaseException:
            process.kill()
            _, status, _ = os.wait4(process.pid, 0)
            process.returncode = os.waitstatus_to_exitcode(status)
            raise
        finally:
            timer.cancel()
            timer.join()
    record = {"command": argv, "exit_code": process.returncode, "timeout": timed_out.is_set(),
              "wall_seconds": time.perf_counter()-start, "user_seconds": usage.ru_utime,
              "system_seconds": usage.ru_stime,
              "peak_rss_bytes": usage.ru_maxrss * (1 if platform.system() == "Darwin" else 1024)}
    if process.returncode == 0 and not timed_out.is_set() and xml.exists():
        root = ET.parse(xml).getroot()
        cases = root.findall(".//testcase")
        record["test_names"] = [case.get("name") for case in cases]
        executed = {name for name in expected_names if any(
            case.get("name", "") == name or case.get("name", "").startswith(name+"/") for case in cases)}
        record["cases_executed"] = len(executed)
        record["junit_case_entries"] = len(cases)  # Catch adds entries for dynamic sections.
        record["assertions"] = sum(int(suite.get("tests", "0")) for suite in root.findall("testsuite"))
        record["valid"] = (executed == set(expected_names) and all(case.get("status") == "run" and
                           all(case.find(tag) is None for tag in ("failure", "error", "skipped")) for case in cases))
    else:
        record["valid"] = False
    (directory / "measurement.json").write_text(json.dumps(record, indent=2)+"\n")
    if not record["valid"]:
        raise RuntimeError(f"Failed or incomplete native workload: {directory}")
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=Path("build/arm64"))
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=5)
    args = parser.parse_args()
    if platform.system() not in ("Darwin", "Linux") or not 3 <= args.repeats <= 20:
        parser.error("Requires macOS/Linux wait4 and 3..20 repeats")
    executables = {}
    for target in ("nonplanar", "fff_print"):
        folder = args.build_dir / "tests" / target / "Release"
        path = folder / f"{target}_tests.app/Contents/MacOS/{target}_tests" if platform.system() == "Darwin" else folder / f"{target}_tests"
        if not path.is_file():
            parser.error(f"Build Release test target first: {path}")
        executables[target] = str(path.resolve())
    args.output_dir.mkdir(parents=True, exist_ok=False)
    output = args.output_dir.resolve()
    workloads = [("contracts_geometry", "nonplanar", "[Nonplanar]~[A07]", 19),
                 ("native_transition", "fff_print", "[A05]", 9),
                 ("serialization_replay", "fff_print", "[A06]", 7),
                 ("simulation_scene", "nonplanar", "[A07]", 6),
                 ("max_candidate_10000_moves", "fff_print", "[A08Stress]", 1)]
    report = {"scope": "BOUNDED_NATIVE_TEST_WORKLOADS_NOT_SYS08", "platform": platform.platform(),
              "machine": platform.machine(), "repeats": args.repeats,
              "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
              "working_diff_sha256": hashlib.sha256(subprocess.check_output(["git", "diff"])).hexdigest(),
              "executable_sha256": {key: digest(Path(value)) for key, value in executables.items()},
              "runner_sha256": digest(Path(__file__)), "workloads": {}}
    if platform.system() == "Darwin":
        report["hardware"] = subprocess.check_output(["/usr/sbin/sysctl", "machdep.cpu.brand_string", "hw.memsize", "hw.ncpu"], text=True).strip()
    for name, target, spec, cases in workloads:
        listing = subprocess.check_output([executables[target], spec, "--list-tests", "--reporter", "xml"], text=True)
        (output / f"{name}-discovery.xml").write_text(listing)
        expected_names = [entry.text for entry in ET.fromstring(listing).findall("TestCase/Name")]
        if len(expected_names) != cases or len(set(expected_names)) != cases:
            raise RuntimeError(f"Unexpected workload discovery for {name}")
        records = []
        for index in range(args.repeats):
            directory = output / f"{name}-{index+1}"
            directory.mkdir()
            records.append(measure([executables[target], spec], directory, expected_names))
        metrics = {key: {"min": min(r[key] for r in records), "median": statistics.median(r[key] for r in records),
                         "max": max(r[key] for r in records)}
                   for key in ("wall_seconds", "user_seconds", "system_seconds", "peak_rss_bytes")}
        report["workloads"][name] = {"cases_per_run": cases, "measurements": records, "metrics": metrics}
        (output / "summary.json").write_text(json.dumps(report, indent=2)+"\n")
        print(name, json.dumps(metrics), flush=True)
    report["status"] = "PASS_BOUNDED_BENCHMARK_ONLY"
    (output / "summary.json").write_text(json.dumps(report, indent=2)+"\n")


if __name__ == "__main__":
    main()
