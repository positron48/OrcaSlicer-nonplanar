#!/usr/bin/env python3
"""Exercise the actual file-capture/native-worker CLI, without printer access."""
import argparse
import hashlib
import json
import struct
import subprocess
import tempfile
from pathlib import Path


def validate(executable, output):
    root = Path(__file__).resolve().parents[2]
    fixtures = root / "docs/nonplanar/fixtures/models"
    output.mkdir(parents=True, exist_ok=False)
    records = []

    def run(name, args, code, status=None, reason=None):
        command = [str(executable.resolve()), *map(str, args)]
        completed = subprocess.run(command, capture_output=True, text=True, timeout=10)
        (output / (name + ".stdout.txt")).write_text(completed.stdout)
        (output / (name + ".stderr.txt")).write_text(completed.stderr)
        record = {"case": name, "command": command, "exit_code": completed.returncode}
        records.append(record)
        assert completed.returncode == code, record
        if status is not None:
            report = json.loads(completed.stdout)
            assert report["kind"] == "stl_geometry_audit" and report["schema_version"] == 1, report
            assert report["status"] == status and report["export_allowed"] is False, report
            assert report["revision"] == 1, report
            if reason is not None:
                assert report["reason"] == reason, report
            record["report"] = report
            return report
        assert not completed.stdout, record

    try:
        for name in ("flat_block", "wedge_5deg"):
            source = fixtures / (name + ".stl")
            before = hashlib.sha256(source.read_bytes()).hexdigest()
            report = run(name, ["--millimeters", source], 0, "VALID_GEOMETRY")
            assert report["source_sha256"] == before == hashlib.sha256(source.read_bytes()).hexdigest()
            assert report["faces"] > 0 and report["peak_rss_bytes"] > 0
            assert 0 < report["volume_lower_mm3"] <= report["volume_upper_mm3"]
            assert report["source_error_upper_mm"] >= 0
            if name == "flat_block":
                assert report["volume_lower_mm3"] <= 2400 <= report["volume_upper_mm3"]
        with tempfile.TemporaryDirectory(prefix="nptop-cli-") as temporary:
            temporary = Path(temporary)
            sphere = fixtures / "shallow_sphere.stl"
            assert struct.unpack_from("<I", sphere.read_bytes(), 80)[0] == 6720
            run("sphere-over-facet-limit", ["--millimeters", sphere], 2, "UNKNOWN", "STL_PARSE_OR_RESOURCE_REJECTION")
            # A separate geometry fixture inside the documented 5000-face domain.
            # Preserve the normative 6720-face input and all baseline goldens.
            catalog = json.loads((fixtures.parent / "catalog.json").read_text())
            model = next(model for model in catalog["models"] if model["id"] == "shallow_sphere").copy()
            model.update(id="shallow_sphere_grid20", grid_xy=[20, 20])
            catalog["models"] = [model]
            catalog_path = output / "small-sphere-catalog.json"
            catalog_path.write_text(json.dumps(catalog, indent=2) + "\n")
            generation = ["python3", str(root / "docs/nonplanar/scripts/generate_fixtures.py"),
                          "--catalog", str(catalog_path), "--out", str(output / "generated")]
            generated = subprocess.run(generation, capture_output=True, text=True, timeout=10, check=True)
            (output / "fixture-generation.json").write_text(json.dumps({"command": generation,
                "exit_code": generated.returncode, "stdout": generated.stdout, "stderr": generated.stderr}, indent=2) + "\n")
            small_sphere = output / "generated/shallow_sphere_grid20.stl"
            before = hashlib.sha256(small_sphere.read_bytes()).hexdigest()
            report = run("small-sphere", ["--millimeters", small_sphere], 0, "VALID_GEOMETRY")
            assert report["faces"] == 1760
            assert report["source_sha256"] == before == hashlib.sha256(small_sphere.read_bytes()).hexdigest()
            malformed = temporary / "malformed.stl"
            malformed.write_bytes(b"not a mesh")
            run("malformed", ["--millimeters", malformed], 2, "UNKNOWN", "STL_PARSE_OR_RESOURCE_REJECTION")
            oversized = temporary / "oversized.stl"
            oversized.write_bytes(b"x" * (2 * 1024 * 1024 + 1))
            run("oversized", ["--millimeters", oversized], 2, "UNKNOWN", "SOURCE_BYTE_LIMIT")
            run("missing", ["--millimeters", temporary / "absent.stl"], 2, "UNKNOWN", "SOURCE_IO_OR_CALLBACK_FAILURE")
            run("directory", ["--millimeters", temporary], 2, "UNKNOWN", "SOURCE_IO_OR_CALLBACK_FAILURE")
            source = (fixtures / "flat_block.stl").read_bytes()
            open_mesh = temporary / "open.stl"
            count = struct.unpack_from("<I", source, 80)[0]
            open_mesh.write_bytes(source[:80] + struct.pack("<I", count - 1) + source[84:-50])
            report = run("open-boundary", ["--millimeters", open_mesh], 2, "INVALID")
            assert report["faces"] == 0 and report["source_error_upper_mm"] is None
            run("undeclared-units", [malformed], 64)
        return {"status": "PASS", "cases": len(records), "records": records}
    finally:
        (output / "commands.json").write_text(json.dumps(records, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(validate(args.executable, args.output_dir), indent=2))
