#!/usr/bin/env python3
"""Exercise the actual file-capture/native-worker CLI, without printer access."""
import argparse
import hashlib
import json
import math
import struct
import subprocess
import tempfile
from pathlib import Path


def hidden_shelf(path):
    """Independent 28-triangle C extrusion: closed solid, two upward sheets."""
    section = [(0, 0), (20, 0), (20, 10), (0, 10), (0, 8), (15, 8), (15, 2), (0, 2)]
    vertices = [(x, y, z) for y in (0, 10) for x, z in section]
    front = [(0, 1, 6), (0, 6, 7), (1, 2, 5), (1, 5, 6), (2, 3, 4), (2, 4, 5)]
    faces = front + [(c + 8, b + 8, a + 8) for a, b, c in front]
    for a in range(8):
        b = (a + 1) % 8
        faces.extend([(a, a + 8, b + 8), (a, b + 8, b)])
    data = bytearray(b"Nonplanar diagnostic hidden shelf".ljust(80, b"\0") + struct.pack("<I", len(faces)))
    for ids in faces:
        a, b, c = [vertices[i] for i in ids]
        u, v = [b[i] - a[i] for i in range(3)], [c[i] - a[i] for i in range(3)]
        normal = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
        length = math.sqrt(sum(n * n for n in normal))
        data.extend(struct.pack("<12fH", *[n / length for n in normal], *a, *b, *c, 0))
    path.write_bytes(data)


def validate(executable, output):
    root = Path(__file__).resolve().parents[2]
    fixtures = root / "docs/nonplanar/fixtures/models"
    output.mkdir(parents=True, exist_ok=False)
    records = []

    def run(name, args, code, status=None, reason=None, kind="stl_geometry_audit"):
        command = [str(executable.resolve()), *map(str, args)]
        completed = subprocess.run(command, capture_output=True, text=True, timeout=10)
        (output / (name + ".stdout.txt")).write_text(completed.stdout)
        (output / (name + ".stderr.txt")).write_text(completed.stderr)
        record = {"case": name, "command": command, "exit_code": completed.returncode}
        records.append(record)
        assert completed.returncode == code, record
        if status is not None:
            report = json.loads(completed.stdout)
            assert report["kind"] == kind and report["schema_version"] == 1, report
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
            report = run(name + "-upper", ["--millimeters", "--upper", source], 0,
                         "NOMINAL_HEIGHTFIELD", "NOMINAL_UPPER_PROJECTION_ONLY", "stl_upper_audit")
            assert report["source_sha256"] == before == hashlib.sha256(source.read_bytes()).hexdigest()
            assert report["frame"] == "source_model" and report["scope"] == "nominal_geometry_only"
            upper = report["upper"]
            assert upper["patches"] == 1 and upper["holes"] == 0
            assert upper["upward_faces"] == upper["selected_faces"]
            expected_area = 600 if name == "flat_block" else 640
            assert upper["area_lower_mm2"] <= expected_area <= upper["area_upper_mm2"]
            assert upper["area_upper_mm2"] - upper["area_lower_mm2"] < 1e-8
            assert 0 <= upper["selected_slope_upper"] <= upper["slope_limit"] == 0.2
            if name == "flat_block":
                assert upper["selected_faces"] == 32 and upper["affine_patches"] == 1 and upper["creases"] == 0
                assert upper["minimum_z_mm"] == upper["maximum_z_mm"] == 4
        report = run("steep-upper-no-selection", ["--millimeters", "--upper", fixtures / "wedge_30deg.stl"], 0,
                     "NOMINAL_HEIGHTFIELD", "NOMINAL_UPPER_PROJECTION_ONLY", "stl_upper_audit")
        assert report["upper"]["upward_faces"] > 0 and report["upper"]["selected_faces"] == 0
        assert report["upper"]["patches"] == 0 and report["upper"]["selected_area_upper_mm2"] == 0
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
            report = run("malformed-upper", ["--millimeters", "--upper", malformed], 2,
                         "UNKNOWN", "STL_PARSE_OR_RESOURCE_REJECTION", "stl_upper_audit")
            assert report["upper"] is None
            shelf = output / "hidden-shelf.stl"
            hidden_shelf(shelf)
            before = hashlib.sha256(shelf.read_bytes()).hexdigest()
            report = run("hidden-shelf-geometry", ["--millimeters", shelf], 0, "VALID_GEOMETRY")
            assert report["faces"] == 28 and report["volume_lower_mm3"] <= 1100 <= report["volume_upper_mm3"]
            report = run("hidden-shelf-upper", ["--millimeters", "--upper", shelf], 2,
                         "UNKNOWN", "OVERLAPPING_UPWARD_PROJECTIONS", "stl_upper_audit")
            assert report["upper"] is None and report["faces"] == 28
            assert report["source_sha256"] == before == hashlib.sha256(shelf.read_bytes()).hexdigest()
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
            run("upper-undeclared-units", ["--upper", malformed], 64)
            run("upper-invalid-option", ["--millimeters", "--unsafe", malformed], 64)
            run("upper-missing-source", ["--millimeters", "--upper"], 64)
        return {"status": "PASS", "cases": len(records), "records": records}
    finally:
        (output / "commands.json").write_text(json.dumps(records, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(validate(args.executable, args.output_dir), indent=2))
