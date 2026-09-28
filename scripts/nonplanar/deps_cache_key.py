#!/usr/bin/env python3
"""Exact Linux dependency-cache identity; generated and untracked files are excluded."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess

INPUT_PATHS = (
    "deps", "cmake", "scripts/linux.d", "build_linux.sh", "CMakeLists.txt", "version.inc",
    "docs/nonplanar/upstream/orca.lock.json", "scripts/nonplanar/deps_cache_key.py",
    ".github/workflows/nonplanar-native.yml",
)
BUILD_COMMAND = ["./build_linux.sh", "-drlL", "-j", "2"]


def command(args, cwd=None):
    return subprocess.check_output(args, cwd=cwd, text=True).strip()


def identity(root, environment):
    entries = subprocess.check_output(
        ["git", "ls-files", "--stage", "-z", "--", *INPUT_PATHS], cwd=root).split(b"\0")
    files = {}
    for entry in filter(None, entries):
        metadata, raw_path = entry.split(b"\t", 1)
        mode, _, stage = metadata.decode().split()
        if stage != "0":
            raise ValueError("Unmerged dependency input")
        path = os.fsdecode(raw_path)
        source = root / path
        data = os.fsencode(os.readlink(source)) if source.is_symlink() else source.read_bytes()
        files[path] = {"mode": mode, "sha256": hashlib.sha256(data).hexdigest()}
    if not files:
        raise ValueError("No tracked dependency inputs")
    payload = {"schema_version": 1, "files": files, "environment": environment,
               "build_command": BUILD_COMMAND,
               # wx-config and CMake packages may embed the installation path.
               "install_prefix": str((root / "deps/build/OrcaSlicer_dep/usr/local").resolve())}
    digest = hashlib.sha256(json.dumps(payload, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    return {"key": "nptop-linux-deps-v1-" + digest, "inputs": payload}


def linux_environment():
    if platform.system() != "Linux" or platform.machine() != "x86_64":
        raise ValueError("This cache is qualified only for the Linux x86_64 workflow")
    image = {key: os.environ[key] for key in ("ImageOS", "ImageVersion")}
    return {
        "platform": platform.system(), "architecture": platform.machine(), "image": image,
        "compiler": command(["/usr/bin/clang++", "--version"]),
        "linker": command(["ld.lld", "--version"]),
        "cmake": command(["cmake", "--version"]), "ninja": command(["ninja", "--version"]),
        "packages": command(["dpkg-query", "-W", "-f=${binary:Package}\t${Version}\t${Architecture}\n"]),
        "build_environment": {key: os.environ.get(key, "") for key in
                              ("CC", "CXX", "CPPFLAGS", "CFLAGS", "CXXFLAGS", "LDFLAGS",
                               "CMAKE_PREFIX_PATH", "PKG_CONFIG_PATH", "DEPS_EXTRA_BUILD_ARGS")},
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--github-output", type=Path, required=True)
    args = parser.parse_args()
    record = identity(Path.cwd(), linux_environment())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(record, indent=2) + "\n")
    with args.github_output.open("a") as stream:
        stream.write("key=" + record["key"] + "\n")
