# Bootstrap execution report — 2026-09-27

Scope: `prompts/01_bootstrap.md`: A01–A03 and concrete A04–A08 tasks. This is a
first C++/test patch, not P1/P2 or a completed Gate A. First physical machine:
Snapmaker U1, model name confirmed by the user; installed configuration and
measurements remain unconfirmed (`A07-snapmaker-u1.md`).

## Source and diff

Upstream base: `8500fcdccaa10b5099ac20d252af3a7c560046f1` (Orca v2.4.2).
Branch: `feature/nonplanar-bootstrap`; bootstrap committed locally, no push/PR.
The fork revision is the commit containing this report and status snapshot:
`git log -1 --format=%H -- docs/nonplanar-dev/status.json` (`SELF` in status.json).
The checks below ran on this patch before committing; the commit is not CI proof.
Commit preparation rechecked package integrity and all six stock/fork cases.
Git attributes preserve original CRLF and raw artifact bytes; narrow ignore
exceptions retain the logs referenced by the manifests. Generated CTest/G-code
whitespace is preserved intentionally; the source/doc patch passes whitespace
checks with the immutable package and raw artifacts excluded.
Incoming directory originally contained the specification only; the pinned
checkout now contains that unchanged package under `docs/nonplanar/`. No Git
submodules exist at the pinned revision. No base/dependency version was advanced.

Production diff: add `src/libslic3r/Nonplanar/Contracts.hpp/.cpp` and two source
entries in libslic3r CMake; add one native test target. Existing slicer, ZAA,
config, writer, GUI and export implementations are untouched. Additional files
are local bootstrap scripts, simulation inputs, first stock goldens, and records.
The new primitives are called by native tests; there is no runtime planner hook.
The application binary SHA-256 remained byte-identical after this additive
module: 9ad1c83ac5f72d1215336a8af53b008e93e29fd4a148d90a3a57cada356600f8.
Thus this differential establishes the bootstrap reference and preservation,
not integration of a new slicing mode.

## Build and actual checks

Platform: macOS 26.5.1 arm64; Apple Clang 21.0.0, Xcode SDK 26.5;
CMake 4.3.1, Ninja 1.13.2. Effective `BUILD_TESTS:BOOL=ON`, native ARM64.
Exact commands, directories, times and exit codes are in `evidence/*.log.json`;
version/dependency details in `evidence/environment.json` and
`evidence/downloaded-dependencies.json`. Raw builds remain under
`build/nonplanar-evidence/`. Reproduction instructions: `build-macos.md`.

| Run / acceptance IDs | Observed result |
|---|---|
| Pinned source audit + negative controls, ORC-01 | PASS; exact commit/13 mapped files, mismatch and missing history reject |
| Dependency build | First exit 2: missing makeinfo. Installed Texinfo; retry exit 0, 622.479 s |
| App configure | First exit 1: missing LZMA headers. Matched pinned script's path flag; exit 0 |
| Unmodified stock app/tools/native tests build | Exit 0, 844 steps, 407.032 s |
| Stock native FFF CTest | **37 discovered, 37 run, 0 failed, 0 skipped**; exit 0 |
| New C++ contracts, ORC-05/VOL-01/VOL-02 + IR/budget | **7 discovered, 7 run**, exit 0; includes 2000 coordinate samples |
| Fork native FFF CTest | **37 discovered, 37 run**, exit 0 |
| OFF/ZAA differential, ORC-02/ORC-03 | Six cases PASS; exact resolved JSON and final commands/order; timestamp-only normalization |
| Bootstrap Python controls, ORC-01/ORC-31 + comparator mutations | 15 tests, exit 0; empty/disabled/unbuilt/failed/skipped selections reject |
| Original package checker/helpers | 61 checksums, 10 analytical STLs; 25 helper tests; exit 0, documentation evidence only |
| Runtime sandbox probe | Local write succeeds, outside write and network denied; exit 0 |
| Separate local dev bundle | Isolated CLI help exit 0, arm64; no external installation or handler registration |

Native CTest discovered 193 upstream cases in total; only the 37-case FFF suite
was selected (plus seven new cases after the patch). The other 156 upstream
cases were NOT_RUN, not silently counted as passed. The 37 names and seven new
names are retained in discovery JSON and JUnit XML in `evidence/`.

The first new-target build deliberately failed before implementation (missing
header). Two later test compilation errors (nan name collision; ambiguous native
Point3 overload) were corrected. A premature CTest run was rejected as empty.
Final full build and real contract tests passed. Failed logs/statuses are retained.

CLI fixture bring-up initially failed because the sandbox blocked Orca's log in
the repository root, then because standalone process compatibility was absent.
Fixed only the local run directory and simulation preset identity/compatibility;
no stock source changes. All accepted runs use fresh local datadirs, own inputs,
no post-process scripts, and a network/write sandbox.

## Stock differential detail

| Model | OFF motion commands | ZAA motion commands | ZAA sloped extrusion commands |
|---|---:|---:|---:|
| Flat block | 1977 | 1977 | 0 |
| 5-degree wedge | 5322 | 7255 | 1586 |
| Shallow sphere | 5708 | 11289 | 4230 |

First untouched stock outputs, input/binary/output hashes, resolved presets and
actual run records are retained in `tests/nonplanar/data/stock-v2.4.2`.
`evidence/stock-fork-diff-reviewed.log` compares those artifacts to the rebuilt
fork. This proves OFF/ZAA preservation for these six runs only. None has been
qualified by the future safety verifier or approved for printing.

## NOT_RUN / remaining risks

- Linux x86_64, remote CI, sanitizers, GUI launch/coexistence and full release
  packaging: NOT_RUN. Homebrew LZMA headers make this build non-hermetic.
- No production nonplanar planner, continuous collision/support checks, body/cap
  partition, final safety verifier or guarded export gate yet.
- Snapshot/cache/schema binding, concurrent scale ownership and whole-plan
  validation remain dependent work; see ADR-0005 and `critical-review.md`.
- A separate author critical pass was performed; independent A08 review remains
  pending. No claim of independent safety approval is made.
- Printer geometry, installed nozzle, firmware transforms/macros and material
  remain unconfirmed. No printer connection, firmware change or print occurred.

Next dependent step: A04 analytical continuous tool/scene geometry, followed by
A05 body reservation and A06 writer/replay. A07 needs operator measurements and
read-only configuration evidence for Snapmaker U1. A08 closes Gate A only after
the real implementation, benchmark and independent review evidence exist.
