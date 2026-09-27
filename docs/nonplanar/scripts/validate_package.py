#!/usr/bin/env python3
"""Validate this specification bundle, NOT a slicer, printer or G-code file.

Standard library is sufficient. --json-schema additionally requires jsonschema.
"""
from __future__ import annotations
import argparse
import ast
import csv
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
from typing import Any
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]


def read_json(path: Path) -> Any:
    def invalid(value: str) -> None:
        raise ValueError(f"Invalid non-finite JSON in {path}: {value}")
    return json.loads(path.read_text(encoding="utf-8"), parse_constant=invalid)


def unique_ids(records: list[dict[str, Any]], name: str) -> dict[str, dict[str, Any]]:
    result = {}
    for record in records:
        key = record["id"]
        if key in result:
            raise ValueError(f"Duplicate {name} id: {key}")
        result[key] = record
    return result


def check_dag(tasks: list[dict[str, Any]]) -> None:
    items = unique_ids(tasks, "task")
    state: dict[str, int] = {}
    def visit(key: str) -> None:
        if key not in items:
            raise ValueError(f"Unknown task dependency: {key}")
        if state.get(key) == 1:
            raise ValueError(f"Dependency cycle at {key}")
        if state.get(key) == 2:
            return
        state[key] = 1
        for dep in items[key]["dependencies"]:
            visit(dep)
        state[key] = 2
    for key in items:
        visit(key)


def content_files(root: Path) -> list[Path]:
    return sorted(p for p in root.rglob("*") if p.is_file() and p.name != "SHA256SUMS.txt"
                  and "__pycache__" not in p.parts and p.suffix != ".pyc" and ".git" not in p.parts)


def verify_checksums(root: Path) -> int:
    manifest = root / "SHA256SUMS.txt"
    if not manifest.is_file():
        raise ValueError("Missing SHA256SUMS.txt")
    expected = {}
    for line in manifest.read_text(encoding="utf-8").splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        if not match:
            raise ValueError(f"Invalid checksum line: {line!r}")
        digest, name = match.groups()
        path = PurePosixPath(name)
        if path.is_absolute() or ".." in path.parts or name in expected or name == "SHA256SUMS.txt":
            raise ValueError(f"Unsafe or duplicate checksum entry: {name}")
        expected[name] = digest
    actual = {p.relative_to(root).as_posix(): p for p in content_files(root)}
    if set(actual) != set(expected):
        raise ValueError(f"Checksum inventory mismatch: missing={sorted(set(expected)-set(actual))}, extra={sorted(set(actual)-set(expected))}")
    for name, path in actual.items():
        if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest() != expected[name]:
            raise ValueError(f"Checksum mismatch or symlink: {name}")
    return len(actual)


def check_links(root: Path) -> int:
    count = 0
    for path in root.rglob("*.md"):
        text = path.read_text(encoding="utf-8")
        for target in re.findall(r"\[[^\]]*\]\(([^)]+)\)", text):
            if re.match(r"^[a-zA-Z]+:", target) or target.startswith("#"):
                continue
            target = unquote(target.split("#", 1)[0])
            dest = (path.parent / target).resolve()
            if not dest.is_relative_to(root.resolve()) or not dest.exists():
                raise ValueError(f"Broken/escaping Markdown link in {path.name}: {target}")
            count += 1
    return count


def validate(root: Path, json_schema: bool = False) -> dict[str, Any]:
    root = root.resolve()
    requirements = unique_ids(read_json(root / "requirements.json")["requirements"], "requirement")
    task_list = read_json(root / "backlog.json")["tasks"]
    tasks = unique_ids(task_list, "task")
    with (root / "acceptance_tests.csv").open(encoding="utf-8-sig", newline="") as stream:
        test_list = list(csv.DictReader(stream))
    tests = unique_ids(test_list, "test")
    headings = {int(n) for n in re.findall(r"^## (\d+)\. ", (root / "SPEC.md").read_text(), re.M)}
    if headings != set(range(1, 37)):
        raise ValueError("SPEC must contain exactly sections 1..36")
    if {r["spec_section"] for r in requirements.values()} != headings:
        raise ValueError("Requirements do not cover SPEC sections")
    check_dag(task_list)
    for rid, req in requirements.items():
        if not req["test_ids"] or not req["task_ids"]:
            raise ValueError(f"Uncovered requirement: {rid}")
        for tid in req["test_ids"]:
            if tid not in tests or rid not in tests[tid]["requirement_ids"].split(";"):
                raise ValueError(f"Broken requirement/test trace: {rid} -> {tid}")
        for tid in req["task_ids"]:
            if tid not in tasks or rid not in tasks[tid]["requirement_ids"]:
                raise ValueError(f"Broken requirement/task trace: {rid} -> {tid}")
    for task in tasks.values():
        if task["status"] != "NOT_STARTED":
            raise ValueError("Brief tasks are a plan; execution evidence belongs in separate run records")
        for rid in task["requirement_ids"]:
            if rid not in requirements:
                raise ValueError(f"Unknown requirement: {rid}")
        for tid in task["test_ids"]:
            if tid not in tests or task["id"] not in tests[tid]["task_ids"].split(";"):
                raise ValueError(f"Broken task/test trace: {task['id']} -> {tid}")
    fixtures = unique_ids(read_json(root / "fixtures/catalog.json")["models"], "fixture")
    for test in tests.values():
        if test["execution_status"] != "NOT_IMPLEMENTED_NOT_RUN":
            raise ValueError("System test plan must not be presented as executed")
        if test["stage"] not in {"P0", "P1", "P2"} or not test["expected"] or not test["oracle"]:
            raise ValueError(f"Incomplete test: {test['id']}")
        for rid in test["requirement_ids"].split(";"):
            if rid not in requirements or test["id"] not in requirements[rid]["test_ids"]:
                raise ValueError(f"Unknown/unlinked requirement from {test['id']}: {rid}")
        for tid in test["task_ids"].split(";"):
            if tid not in tasks or test["id"] not in tasks[tid]["test_ids"]:
                raise ValueError(f"Unknown/unlinked owner for {test['id']}: {tid}")
        for fid in filter(None, test["fixture_ids"].split(";")):
            if fid not in fixtures:
                raise ValueError(f"Unknown fixture: {fid}")
    json_files = list(root.rglob("*.json"))
    for path in json_files:
        read_json(path)
    python_files = list((root / "scripts").glob("*.py"))
    for path in python_files:
        ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    profile = read_json(root / "profiles/generic_conservative_unverified.json")
    if profile["usage_scope"] != "simulation_only" or profile["confirmation_status"] != "unverified":
        raise ValueError("Example toolhead must remain simulation-only and unconfirmed")
    if profile["nozzle"]["tip_outer_diameter_mm"] <= profile["nozzle"]["orifice_diameter_mm"]:
        raise ValueError("Example must explicitly include a finite metal nozzle tip")
    report = read_json(root / "examples/validation-unknown.json")
    if not report["document_example"] or report["export_decision"] != "BLOCK":
        raise ValueError("Example report must not permit printing")
    lock = read_json(root / "upstream/orca.lock.json")
    source_map = read_json(root / "upstream/source_map.json")
    if not re.fullmatch(r"[a-f0-9]{40}", lock["commit"]) or source_map["commit"] != lock["commit"]:
        raise ValueError("Invalid or inconsistent upstream commit")
    fixture_manifest = read_json(root / "fixtures/models/manifest.json")
    if {m["id"] for m in fixture_manifest["models"]} != set(fixtures):
        raise ValueError("Generated fixture inventory differs from catalog")
    import struct
    for model in fixture_manifest["models"]:
        payload = (root / "fixtures/models" / model["file"]).read_bytes()
        if hashlib.sha256(payload).hexdigest() != model["sha256"]:
            raise ValueError(f"Fixture checksum mismatch: {model['id']}")
        if len(payload) != 84 + struct.unpack_from("<I", payload, 80)[0] * 50:
            raise ValueError(f"Invalid binary STL length: {model['id']}")
    schema_result = "NOT_REQUESTED"
    if json_schema:
        try:
            from jsonschema import Draft202012Validator
        except ImportError as exc:
            raise ValueError("--json-schema requires optional package jsonschema") from exc
        pairs = [("toolhead-profile", "profiles/generic_conservative_unverified.json"),
                 ("project-extension", "examples/project-extension.json"),
                 ("validation-report", "examples/validation-unknown.json")]
        for schema_name, example in pairs:
            schema = read_json(root / f"schemas/{schema_name}.schema.json")
            Draft202012Validator.check_schema(schema)
            Draft202012Validator(schema).validate(read_json(root / example))
        schema_result = "PASS_3_DRAFT_SCHEMAS_AND_EXAMPLES"
    link_count = check_links(root)
    file_count = verify_checksums(root)
    if any(p.suffix.lower() in {".gcode", ".bgcode"} for p in content_files(root)):
        raise ValueError("This brief must not contain ready-to-print G-code")
    return {"status": "PASS_DOCUMENTATION_PACKAGE_ONLY", "requirements": len(requirements),
            "planned_system_tests": len(tests), "planned_tasks": len(tasks), "analytic_stl_models": len(fixtures),
            "checksum_files": file_count, "markdown_links_checked": link_count,
            "json_files_parsed": len(json_files), "python_files_syntax_checked": len(python_files),
            "json_schema_validation": schema_result,
            "orca_build": "NOT_RUN", "slicer_tests": "NOT_IMPLEMENTED_NOT_RUN", "physical_tests": "NOT_RUN"}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--json-schema", action="store_true")
    args = parser.parse_args()
    try:
        result = validate(args.root, args.json_schema)
    except Exception as exc:
        parser.exit(2, f"Package validation failed: {exc}\n")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
