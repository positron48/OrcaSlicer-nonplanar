#!/usr/bin/env python3
"""Generate analytic geometry-only STL fixtures. Never generates printer commands.

Python 3.10+, standard library only. The STL surface approximates the formula;
no general chord-error or printability certificate is implied by generation.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import struct
from typing import Any

Vec3 = tuple[float, float, float]
Triangle = tuple[Vec3, Vec3, Vec3]
SHAPES = {"flat", "wedge", "cylinder", "sphere", "dome", "saddle", "sine", "double_bump"}
ROOT = Path(__file__).resolve().parents[1]


def load_catalog(path: Path) -> dict[str, Any]:
    def bad_constant(s: str) -> None:
        raise ValueError(f"Non-finite JSON constant: {s}")
    data = json.loads(path.read_text(encoding="utf-8"), parse_constant=bad_constant)
    if data.get("schema_version") != 1 or not isinstance(data.get("models"), list):
        raise ValueError("Unsupported catalog or missing models list")
    seen: set[str] = set()
    for model in data["models"]:
        name = model.get("id", "")
        if not re.fullmatch(r"[a-z0-9_]{1,64}", name) or name in seen:
            raise ValueError(f"Unsafe or duplicate fixture id: {name!r}")
        seen.add(name)
        if model.get("shape") not in SHAPES:
            raise ValueError(f"Unknown analytic shape for {name}")
        extent = model.get("extent_xy_mm", [])
        grid = model.get("grid_xy", [])
        if len(extent) != 2 or any(not isinstance(x, (int, float)) or not math.isfinite(x) or x <= 0 for x in extent):
            raise ValueError(f"Invalid extent for {name}")
        if len(grid) != 2 or any(type(x) is not int or not 2 <= x <= 128 for x in grid):
            raise ValueError(f"Invalid grid for {name}; each count must be 2..128")
        params = model.get("parameters", {})
        if not isinstance(params, dict) or any(not isinstance(x, (int, float)) or not math.isfinite(x) for x in params.values()):
            raise ValueError(f"Invalid numeric parameters for {name}")
    if not seen:
        raise ValueError("Empty model catalog")
    return data


def height(model: dict[str, Any], x: float, y: float) -> float:
    p = model["parameters"]
    lx, ly = model["extent_xy_mm"]
    b = p["height_mm"]
    shape = model["shape"]
    if shape == "flat":
        z = b
    elif shape == "wedge":
        z = b + math.tan(math.radians(p["angle_deg"])) * (x + lx / 2)
    elif shape == "cylinder":
        radicand = p["radius_mm"] ** 2 - x * x
        if radicand <= 0:
            raise ValueError("Cylinder domain extends beyond radius")
        z = b + math.sqrt(radicand) - p["radius_mm"]
    elif shape == "sphere":
        radicand = p["radius_mm"] ** 2 - x * x - y * y
        if radicand <= 0:
            raise ValueError("Sphere domain extends beyond radius")
        z = b + math.sqrt(radicand) - p["radius_mm"]
    elif shape == "dome":
        z = b + math.sqrt(max(0.0, p["radius_mm"] ** 2 - x * x - y * y))
    elif shape == "saddle":
        z = b + p["coefficient_per_mm"] * (x * x - y * y)
    elif shape == "sine":
        z = b + p["amplitude_mm"] * math.sin(2 * math.pi * x / lx) * math.cos(2 * math.pi * y / ly)
    elif shape == "double_bump":
        sigma = p["sigma_mm"]
        if sigma <= 0:
            raise ValueError("sigma_mm must be positive")
        z = b + sum(p["amplitude_mm"] * math.exp(-((x - center) ** 2 + y * y) / sigma ** 2)
                    for center in (-p["center_offset_mm"], p["center_offset_mm"]))
    else:
        raise ValueError(f"Unknown shape: {shape}")
    if not math.isfinite(z) or z <= 0:
        raise ValueError(f"Non-finite or non-positive surface height: {z}")
    return z


def mesh(model: dict[str, Any]) -> list[Triangle]:
    nx, ny = model["grid_xy"]
    if any(type(v) is not int or not 2 <= v <= 128 for v in (nx, ny)):
        raise ValueError("Grid out of bounds")
    lx, ly = model["extent_xy_mm"]
    top: list[Vec3] = []
    bottom: list[Vec3] = []
    for j in range(ny + 1):
        for i in range(nx + 1):
            x = -lx / 2 + lx * i / nx
            y = -ly / 2 + ly * j / ny
            top.append((x, y, height(model, x, y)))
            bottom.append((x, y, 0.0))
    index = lambda i, j: j * (nx + 1) + i
    triangles: list[Triangle] = []
    for j in range(ny):
        for i in range(nx):
            a, b, c, d = index(i, j), index(i + 1, j), index(i + 1, j + 1), index(i, j + 1)
            triangles += [(top[a], top[b], top[c]), (top[a], top[c], top[d]),
                          (bottom[a], bottom[c], bottom[b]), (bottom[a], bottom[d], bottom[c])]
    # CCW boundary in XY: each side face then points outwards.
    boundary = ([index(i, 0) for i in range(nx)] +
                [index(nx, j) for j in range(ny)] +
                [index(i, ny) for i in range(nx, 0, -1)] +
                [index(0, j) for j in range(ny, 0, -1)])
    for n, a in enumerate(boundary):
        b = boundary[(n + 1) % len(boundary)]
        triangles += [(bottom[a], bottom[b], top[b]), (bottom[a], top[b], top[a])]
    return triangles


def normal(t: Triangle) -> Vec3:
    a, b, c = t
    u = tuple(b[k] - a[k] for k in range(3))
    v = tuple(c[k] - a[k] for k in range(3))
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    length = math.sqrt(sum(q * q for q in n))
    if not math.isfinite(length) or length == 0:
        raise ValueError("Degenerate triangle")
    return tuple(q / length for q in n)  # type: ignore[return-value]


def encode_stl(name: str, triangles: list[Triangle]) -> bytes:
    header = ("NPTOP GEOMETRY ONLY; NOT GCODE; " + name).encode("ascii")[:80].ljust(80, b" ")
    out = bytearray(header + struct.pack("<I", len(triangles)))
    for tri in triangles:
        vals = (*normal(tri), *tri[0], *tri[1], *tri[2])
        out.extend(struct.pack("<12fH", *vals, 0))
    return bytes(out)


def read_stl(data: bytes) -> list[Triangle]:
    if len(data) < 84:
        raise ValueError("Truncated binary STL")
    count = struct.unpack_from("<I", data, 80)[0]
    if len(data) != 84 + count * 50:
        raise ValueError("Invalid binary STL byte count")
    tris = []
    for i in range(count):
        vals = struct.unpack_from("<12fH", data, 84 + i * 50)
        if not all(math.isfinite(q) for q in vals[:12]):
            raise ValueError("Non-finite STL data")
        tris.append((tuple(vals[3:6]), tuple(vals[6:9]), tuple(vals[9:12])))
    return tris  # type: ignore[return-value]


def signed_volume(tris: list[Triangle]) -> float:
    terms = []
    for a, b, c in tris:
        cross = (b[1] * c[2] - b[2] * c[1], b[2] * c[0] - b[0] * c[2], b[0] * c[1] - b[1] * c[0])
        terms.append(sum(a[k] * cross[k] for k in range(3)) / 6)
    return math.fsum(terms)


def generate(catalog: dict[str, Any], out: Path) -> dict[str, Any]:
    if out.is_symlink():
        raise ValueError("Output directory must not be a symlink")
    out.mkdir(parents=True, exist_ok=True)
    records = []
    for model in catalog["models"]:
        name = model["id"]
        if not re.fullmatch(r"[a-z0-9_]{1,64}", name):
            raise ValueError("Unsafe model name")
        payload = encode_stl(name, mesh(model))
        target = out / (name + ".stl")
        if target.is_symlink():
            raise ValueError("Output file must not be a symlink")
        target.write_bytes(payload)
        decoded = read_stl(payload)
        vertices = [v for tri in decoded for v in tri]
        records.append({"id": name, "file": target.name, "sha256": hashlib.sha256(payload).hexdigest(),
                        "triangles": len(decoded), "bytes": len(payload),
                        "mesh_volume_mm3": signed_volume(decoded),
                        "bbox_min_mm": [min(v[k] for v in vertices) for k in range(3)],
                        "bbox_max_mm": [max(v[k] for v in vertices) for k in range(3)],
                        "status": "GEOMETRY_ONLY_NO_PRINTER_QUALIFICATION"})
    report = {"schema_version": 1, "generator": "generate_fixtures.py", "models": records,
              "analytic_chord_error_certified": False, "gcode_generated": False}
    (out / "manifest.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--catalog", type=Path, default=ROOT / "fixtures/catalog.json")
    parser.add_argument("--out", type=Path, default=ROOT / "fixtures/models")
    args = parser.parse_args()
    try:
        report = generate(load_catalog(args.catalog), args.out)
    except (OSError, ValueError, KeyError, TypeError, struct.error) as exc:
        parser.exit(2, f"Fixture generation failed: {exc}\n")
    print(json.dumps({"generated_geometry_models": len(report["models"]), "out": str(args.out), "gcode_generated": False}))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
