#!/usr/bin/env python3
"""Expose bounded native test failures in public GitHub check annotations."""
import argparse
from pathlib import Path
import xml.etree.ElementTree as ET


def annotation(message):
    # Treat all log/test text as data, including embedded workflow commands.
    message = message.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
    return "::error title=Nonplanar native tests::" + message


def diagnostics(evidence):
    messages = []
    log = evidence / "tests.log"
    if log.is_file():
        with log.open("rb") as source:
            source.seek(max(0, log.stat().st_size - 65536))
            tail = source.read(65536).decode("utf-8", errors="replace")
        failures = [line for line in tail.splitlines() if "CTest gate failed:" in line]
        if failures:
            messages.append(failures[-1][:2048])
    report = evidence / "ctest" / "results.xml"
    if report.is_file():
        try:
            if report.stat().st_size > 8 * 1024 * 1024:
                raise ValueError("JUnit report exceeds diagnostic size limit")
            cases = ET.parse(report).getroot().findall(".//testcase")
            failed = [case for case in cases if case.get("status") != "run" or
                      any(case.find(tag) is not None for tag in ("failure", "error", "skipped"))]
            for case in failed[:5]:
                details = "\n".join("".join(node.itertext()) for node in case
                                    if node.tag in ("failure", "error", "skipped", "system-out"))
                messages.append((case.get("name", "Unnamed test")[:256] + "\n" + details[:1792]).rstrip())
            if len(failed) > 5:
                messages.append(f"{len(failed) - 5} additional failed/skipped tests; see uploaded JUnit artifact")
        except (OSError, ET.ParseError, ValueError) as exc:
            messages.append(f"Cannot read native test report: {exc}")
    if not messages:
        messages.append("Native test step failed before usable CTest diagnostics; inspect uploaded command/exit records")
    return [annotation(message) for message in messages]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    print("\n".join(diagnostics(args.evidence_dir)))
