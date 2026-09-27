#!/usr/bin/env python3
"""Run an explicit argv and retain its exit status and output as build evidence."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, required=True)
parser.add_argument("command", nargs=argparse.REMAINDER)
args = parser.parse_args()
command = args.command[1:] if args.command[:1] == ["--"] else args.command
if not command:
    parser.error("a command is required after --")
args.output.parent.mkdir(parents=True, exist_ok=True)
record = {
    "command": command, "cwd": str(Path.cwd()),
    "started_utc": datetime.now(timezone.utc).isoformat(), "status": "RUNNING",
}
metadata = args.output.with_suffix(args.output.suffix + ".json")


def save_record():
    temporary = metadata.with_suffix(metadata.suffix + ".tmp")
    temporary.write_text(json.dumps(record, indent=2) + "\n")
    temporary.replace(metadata)


with args.output.open("xb") as stream:
    save_record()
    start = time.monotonic()
    try:
        result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT)
        record.update(status="COMPLETED", exit_code=result.returncode)
    except (OSError, KeyboardInterrupt) as exc:
        record.update(status="ERROR", exit_code=130 if isinstance(exc, KeyboardInterrupt) else 127,
                      error=str(exc))
    record["elapsed_seconds"] = round(time.monotonic() - start, 3)
    save_record()
print(json.dumps(record))
raise SystemExit(record["exit_code"])
