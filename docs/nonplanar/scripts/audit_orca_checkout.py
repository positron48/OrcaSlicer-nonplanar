#!/usr/bin/env python3
"""Read-only local Git/source inventory audit. Does not build or validate a slicer."""
from __future__ import annotations
import argparse
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
from typing import Any

ROOT = Path(__file__).resolve().parents[1]


def git(repo: Path, *args: str) -> str:
    result = subprocess.run(["git", "-C", str(repo), *args], check=False, capture_output=True, text=True, timeout=30)
    if result.returncode:
        raise ValueError(f"git {' '.join(args)} failed: {result.stderr.strip()}")
    return result.stdout.strip()


def audit_checkout(repo: Path, lock: dict[str, Any], source_map: dict[str, Any]) -> dict[str, Any]:
    baseline = lock.get("commit", "")
    if not re.fullmatch(r"[a-f0-9]{40}", baseline):
        raise ValueError("Expected full hexadecimal baseline commit in lock")
    if source_map.get("commit") != baseline:
        raise ValueError("Source map and lock use different commits")
    root = Path(git(repo, "rev-parse", "--show-toplevel")).resolve()
    resolved = git(root, "rev-parse", "--verify", baseline + "^{commit}")
    if resolved != baseline:
        raise ValueError("Baseline commit object mismatch")
    git(root, "merge-base", "--is-ancestor", baseline, "HEAD")
    head = git(root, "rev-parse", "HEAD")
    records = []
    for row in source_map["files"]:
        path = PurePosixPath(row["path"])
        if path.is_absolute() or ".." in path.parts:
            raise ValueError("Unsafe path in source map")
        baseline_text = git(root, "show", f"{baseline}:{path.as_posix()}")
        current_path = root / path
        if not current_path.is_file() or current_path.is_symlink():
            raise ValueError(f"Missing or symlinked current source: {path}")
        current_text = current_path.read_text(encoding="utf-8", errors="strict")
        for marker in row.get("markers", []):
            if marker not in baseline_text:
                raise ValueError(f"Baseline marker absent: {path} : {marker}")
            if marker not in current_text:
                raise ValueError(f"Working-tree marker absent; re-audit changed hook: {path} : {marker}")
        records.append({"path": str(path), "markers_checked": len(row.get("markers", [])), "status": "PRESENT"})
    return {"status": "PASS_SOURCE_INVENTORY_ONLY", "baseline_commit": baseline, "head_commit": head,
            "head_is_baseline": head == baseline, "files": records,
            "tracked_worktree_changes": git(root, "status", "--porcelain", "--untracked-files=no").splitlines(),
            "build_status": "NOT_RUN", "slicer_test_status": "NOT_RUN", "semantic_hook_audit": "REQUIRES_ENGINEER_REVIEW"}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--repo", type=Path, required=True)
    ap.add_argument("--lock", type=Path, default=ROOT / "upstream/orca.lock.json")
    ap.add_argument("--source-map", type=Path, default=ROOT / "upstream/source_map.json")
    args = ap.parse_args()
    try:
        report = audit_checkout(args.repo, json.loads(args.lock.read_text()), json.loads(args.source_map.read_text()))
    except (OSError, ValueError, KeyError, subprocess.TimeoutExpired) as exc:
        ap.exit(2, f"Source inventory audit failed: {exc}\n")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
