# Native macOS bootstrap

Working directory: repository root. Base SHA:
`8500fcdccaa10b5099ac20d252af3a7c560046f1`. Native arm64, macOS 26.5.1,
Xcode SDK 26.5, Apple Clang 21.0.0, CMake 4.3.1, Ninja 1.13.2.
No Git submodules are declared at this revision. Dependencies are defined in
`deps/*.cmake` and individual component manifests with source hashes/patches.

## Commands actually used

```sh
python3 docs/nonplanar/scripts/validate_package.py
python3 -m unittest discover -s docs/nonplanar/scripts -p 'test_*.py' -v
python3 docs/nonplanar/scripts/audit_orca_checkout.py --repo .
env CMAKE_BUILD_PARALLEL_LEVEL=4 ./build_release_macos.sh -d -x -a arm64 -t 11.3
```

First dependency attempt exited 2: MPFR's documentation build could not find
`makeinfo`. The pinned workflow lists Texinfo as a prerequisite. Installed
Texinfo 7.3_1 with `HOMEBREW_NO_AUTO_UPDATE=1 brew install texinfo` (exit 0).
Homebrew performed its default cache cleanup during installation. No Orca
source, dependency version or pinned commit was changed.

```sh
env PATH="/opt/homebrew/opt/texinfo/bin:$PATH" CMAKE_BUILD_PARALLEL_LEVEL=4 \
  ./build_release_macos.sh -d -x -a arm64 -t 11.3
```

The dependency install prefix is confined to
`deps/build/arm64/OrcaSlicer_dep/usr/local`. Do not relocate it: wx-config stores
the absolute prefix. The pinned script supplies CMake 4 compatibility flags
itself. Large per-dependency builds can continue without new top-level output;
their `.ninja_log` provides observed progress.

The repeated dependency build completed with exit 0 in 622.479 seconds.
wxWidgets tag v3.3.2 resolved to
`88f3483ca546fbf4ad732e1acd94cc930935077a`. Downloaded archive hashes are in
`build/nonplanar-evidence/downloaded-dependencies.json`.

The first app configure used semicolon-separated `CMAKE_IGNORE_PREFIX_PATH`
and exited 1 because the LZMA include directory was missing. The pinned deps
script had used a colon-separated value and Boost had selected Homebrew xz
5.8.3 headers with the macOS SDK `liblzma.tbd`. Matching the script's original
value configured successfully (exit 0); no version/source patch was made.
This system-header dependency is part of the observed environment, not a
fully hermetic dependency bundle.

```sh
cmake -S . -B build/arm64 -G 'Ninja Multi-Config' \
  -DORCA_TOOLS=ON -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.3 \
  -DCMAKE_IGNORE_PREFIX_PATH=/opt/local:/usr/local:/opt/homebrew \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/arm64 --config Release --target all --parallel 8
```

Effective `build/arm64/CMakeCache.txt` confirms `BUILD_TESTS:BOOL=ON` and
`CMAKE_OSX_ARCHITECTURES:STRING=arm64`. The main source selects its own CMake
policy minimum. Build completion and native test results must be read from
the current run evidence, not inferred from configure success.

## Test acceptance

`scripts/nonplanar/ctest_gate.py` requires actual `BUILD_TESTS:BOOL=ON`,
nonempty CTest discovery with executable commands and a successful matching
JUnit result with no skipped tests. Use its `--regex` only with the precise
selection recorded in the run report. This gate does not claim the entire
upstream suite passed when a subset was selected.

```sh
python3 -m unittest discover -s scripts/nonplanar -p 'test_*.py' -v
```

These Python tests exercise ORC-01 source auditing and ORC-31 CTest acceptance;
they are not Orca slicing tests. Raw command/output/exit evidence is retained
under `build/nonplanar-evidence/`, with summary records in this directory.

## Runtime isolation

Launch only the built executable with an explicit repository-local `--datadir`;
use a local temporary directory and block network for baseline execution.
Do not install over `/Applications/OrcaSlicer.app`. Native GUI/bundle/cache
isolation must be verified before declaring A01 complete. Do not run printer
connections, imported scripts or macros.

## Observed completion and follow-up commands

Stock application/tools/test binaries built with exit 0 in 407.032 seconds.
37 native FFF cases ran successfully in stock and again after the C++ patch.
Seven new native contract cases pass. The other 156 upstream cases were not
selected. Exact selections and JUnit results are in `evidence/`.

```sh
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 \
  --output-dir build/nonplanar-evidence/contracts-rerun \
  --regex '^(ORC-05|VOL-01|VOL-02|IR deposition|Numeric budgets)'
python3 scripts/nonplanar/capture_baselines.py --label next-baseline-run
python3 scripts/nonplanar/compare_baselines.py \
  tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/next-baseline-run
```

Use a fresh capture label: the tool refuses to overwrite prior evidence.
Accepted baseline capture runs are native CLI, with absolute paths, cwd/log/
datadir/tmp confined to their per-run directory and an OS write/network sandbox.
The standalone simulation machine uses Orca's `from: system` identity semantics
so the process's explicit compatible_printers entry matches. This does not
assert measurement, safety approval or installation as a vendor preset.

`prepare_dev_bundle.py` created a separate local app identity and executable:
`build/nonplanar-dev/NonplanarTopLab.app`. Document/URL handlers are removed.
Its launcher owns datadir/temp/cache, sets cwd and enforces the offline write
sandbox, including on a launch without --datadir. Direct CLI `--help` returned 0.
The resources link points at this checkout, so this is not a portable package.
GUI launch/coexistence and signing/release packaging remain NOT_RUN; no Finder
registration or /Applications install was performed. Only CLI isolation is
accepted in this bootstrap. Do not bypass the launcher for user-facing runs.

Later GUI validation is recorded in `A01-gui.md`: fresh labeled bundles, actual
OS isolation probes and native import/slice/preview pass on 2026-09-28. The
bootstrap NOT_RUN statement above is historical. Stock coexistence and portable
release packaging remain unverified; no /Applications installation occurred.

`A01-coexistence.md` records simultaneous isolated native processes on
2026-09-28. Interactive stock GUI coexistence remains unverified: wrapper
automation failed, and a disposable app copy was rejected during launch with
a failing signature check. All test processes were stopped; the installed
stock executable stayed unchanged. No OS protection bypass was attempted.
