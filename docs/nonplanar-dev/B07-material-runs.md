# B07 continuous-run lower material

Invariant: the entire requested box, expanded by the original inner XY/Z and
numerical losses, lies in the actual nominal continuous extrusion run. Its
finite real ends and actual current front remain eroded. Internal packet cuts
retain both adjacent actual flux/sections; no per-event lower erosion is removed.
ADR-0055 defines additive protected material-run contract 1 and its declared
continuous-run assumption. It is not a calibrated physical extrusion model.

Three new analytical cases / 2659 assertions pass. Both X/Y axes,
directions and rectangle/rounded sections retain actual varying packet amounts,
gap and Z. Current partial material is admitted and future material is absent.
Short individual lower sets can remain empty while the independently certified
expanded box spans a continuous run. Real ends, insufficient current length,
reversal, turn, travel, foreign provenance, narrow neighbours and raised floors
refuse. Numerical coordinate error is charged separately from the inner loss.
Zero-thickness coverage planes are supported; joins still need positive volume.

An independent 113-bit oracle bounds every whole expanded longitudinal slice
using the actual binary amount divided by original packet length, affine
floor/top/centre and minimum local core/radius. It does not call the production
predicate or replace continuous bounds with vertex samples. Original per-event
join tests and their independent derivative/finite-butt oracle remain.

The unchanged native wedge uses the same actual sliced body, 0.6/0.75 mm ROIs,
0.4 mm cap, five paths and 161 packets. All six local run-level joins pass with
45 shared cells / 731 work, minimum certified box approximately 0.000724661 mm3.
A whole long-run box passes through 53 packets with 53 cells / 106 work. The
extended native case passes 1455 assertions. The original full per-event test
still refuses paths 2/3 with 64 cells / 623 work and nine independently proven
empty lower packets. Its high-corner box remains approximately 3.44447e-5 mm3
with 10 cells / 22 work. Original S/U/R/C/M/spill assertions remain unchanged.

Search and nominal slice refinement share the original 4095 cells / 200000 work,
depth 32 / one-second join deadline, minimum box 1e-6 mm3 and publication guards.
Nested refinement uses remaining search depth. Capture work is charged to the
same root; callbacks also retain the root deadline during capture. Exhausted
work/cells, invalid depth/domain, stale/cancelled/late publication, nonstandard
rounding and timeout provide no certificate. Mutable caller wrappers and limits
cannot alter captured input. Whole-cap publication is complete or absent.

Original cap amount error 1e-4 mm3, gap error 1e-4 mm, width error 0.002 mm, fill
precision 0.001 mm3, original band/losses and 20-second native-fill deadline are
retained. No body, geometry, target, actual path/amount, profile, public default,
CMake, dependency, golden or normative bundle changes. Rebuild all consumers;
new protected snapshots own their original immutable source. There is no new
persisted cache, IR, 3MF or profile schema.

Apple Clang 21, macOS 26.5.1 ARM64 Release application/native targets build.
Final material suites: 83 cases / 19592 assertions. Actual-body
suites: 14 cases / 109343 assertions. Selected CTest: 328/328,
without skips. Six fresh OFF/ZAA G-code and modal-replay comparisons pass against
the pinned upstream with original normalization/additive OFF-default allowance.
Source inventory and original package validation pass. The missing-API red
build is retained with exit 1; no failed build is counted as verification.
Build5 is the final application/test-target build. Final confirmations repeat
after remaining-depth and separate numerical-loss/coverage-plane checks.

Saved 16:23 UTC Linux observations: joint-cap run 36878746418 at a165f0c3 succeeds
in native suites, STL CLI and application. Material-join run 36887521770 at
d02d7544 is in progress. This new run contract awaits its own Linux verification.
Independent safety review, Windows and physical qualification remain NOT_RUN.

Next: qualify allowable excess, finite-width floor/body bonding and volumetric
seam, construct remaining 3D fill and later actual curved/stepped support. These
local witnesses do not prove a completely connected lower layer, bonding,
head access/contact, executable order/flow or export permission. Full B01-B15
remains active and public export stays BLOCK.

Exact command argv, exits, XML, source/binary/raw/archive hashes and deterministic
gzip mappings live in `evidence/B07-material-runs/`. Raw results:
`build/nonplanar-evidence/B07-material-runs`; final baseline capture:
`build/nonplanar-evidence/B07-material-runs-confirmation-baselines`. Preliminary
build3 baselines are retained separately and do not bind the final binaries.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialRun],[MaterialRunJoin]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeMaterialRun]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-material-runs/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-material-runs-confirmation-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-material-runs-confirmation-baselines --allow-nptop-off-default
```

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-material-runs.md`.
Diff: `git show <resolved-implementation-revision>`.
