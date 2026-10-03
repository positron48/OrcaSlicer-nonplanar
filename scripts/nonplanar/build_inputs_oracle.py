#!/usr/bin/env python3
"""Independently compare an embedded build-input inventory with checkout bytes.

This checks the inventory, not object-file provenance or runtime dependencies.
"""
import argparse
import hashlib
import json
from pathlib import Path


def input_paths(root):
    paths = set()
    for scope in ("src", "deps_src", "resources", "cmake", "deps"):
        for path in (root / scope).rglob("*"):
            relative = path.relative_to(root)
            if not path.is_file() or any(part in ("build", "DL_CACHE", ".git") for part in relative.parts):
                continue
            if path.name == ".DS_Store":
                continue
            if scope == "deps" and path.suffix != ".cmake" and path.name != "CMakeLists.txt":
                continue
            paths.add(relative.as_posix())
    for pattern in ("build*.sh", "build*.bat", "build*.ps1"):
        paths.update(path.relative_to(root).as_posix() for path in root.glob(pattern) if path.is_file())
    paths.update(("CMakeLists.txt", "version.inc", "docs/nonplanar/upstream/orca.lock.json"))
    return sorted(paths)


def verify(inventory, root):
    raw = inventory.read_bytes()
    value = json.loads(raw)
    assert set(value) == {"build", "files", "schema", "scope"}
    assert value["schema"] == 1 and value["scope"] == "build-input-inventory"
    assert raw == json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode()
    expected = [[name, hashlib.sha256((root / name).read_bytes()).hexdigest()] for name in input_paths(root)]
    assert value["files"] == expected, "inventory differs from actual checkout files"
    assert all(isinstance(item, str) for item in value["build"].values())
    return {"status": "PASS", "scope": value["scope"], "files": len(expected),
            "sha256": hashlib.sha256(raw).hexdigest(), "bytes": len(raw),
            "software_identity": "UNKNOWN/NOT_RUN"}


def verify_binding(inventory, report):
    raw = inventory.read_bytes()
    record = json.loads(report.read_bytes())
    canonical = record["job_canonical"].encode()
    assert hashlib.sha256(canonical).hexdigest() == record["job_fingerprint"]
    job = json.loads(canonical)
    software = [item for item in job["resources"] if item[0] == 7]
    assert len(software) == 1, "job must have exactly one software resource"
    kind, name, digest, length = software[0]
    assert bytes.fromhex(name).decode() == "compiled-build-inputs-v1"
    assert bytes.fromhex(digest).decode() == hashlib.sha256(raw).hexdigest()
    assert length == len(raw)
    return {"report": report.name, "job_fingerprint": record["job_fingerprint"], "status": "PASS"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inventory", type=Path)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--job-report", action="append", type=Path, default=[])
    args = parser.parse_args()
    result = verify(args.inventory, args.root)
    result["job_bindings"] = [verify_binding(args.inventory, report) for report in args.job_report]
    print(json.dumps(result))
