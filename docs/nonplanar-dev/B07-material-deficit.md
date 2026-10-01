# B07 localized deficit lower witnesses

Invariant: locate regions with rigorously positive missing target volume without
mistaking overcounted candidate amounts or a zero witness for complete fill.
ADR-0044 adds protected internal material-deficit contract 1. The restricted
change scope is DepositionModel, its geometric/native tests, this ADR/report
and execution records. Existing material/IR/project/profile serialization,
fixtures, dependencies and export routes are unchanged.

Three new analytical cases / 118 assertions pass. Independent flat-volume and
rotated finite-butt examples test fractions 0/0.3/0.5/1 and absent future records.
The unprinted half has positive witnesses; an exactly filled example has zero
lower witnesses. Exact clipping excludes the upper-right quadrant beyond a
rotated current butt, despite a future bead in the ledger. Snapshot/limits/grid
ownership survives callback mutation. Missing source, incomplete/unordered/
nonfinite cuts, region/work/fragment limits, coarse precision, cancellation,
stale revision, timeout and changed rounding publish no snapshot.

The existing native first-hatch case is extended from 54 to 64 assertions.
Its protected actual 1:16 wedge fill proof is partitioned by X=[19,20,21] and
Y=[17,20,23] mm. All four regions have positive missing-volume lower witnesses:

| XY rectangle in mm | Missing volume at least, mm3 |
|---|---:|
| [19,20] x [17,20] | 0.18175706739469458 |
| [20,21] x [17,20] | 0.30987696930664688 |
| [19,20] x [20,23] | 0.18150519704347665 |
| [20,21] x [20,23] | 0.31015067857299378 |

The outward summed lower witness is 0.98328991231781182 mm3. The original
whole deficit remains approximately [1.27173,1.28710] mm3: the witnesses locate
a strict lower part, not the entire missing geometry. The complete target-grid
interval width is 0.00099991331547766 mm3. The map uses 11307 proof fragments /
29244 evaluations, under 65535 fragments, 200000 evaluations, 0.02 mm3 target
precision and a five-second cooperative deadline. Whole native-case time is
28.555 seconds in the final body suite, including unchanged capture/planning/
union/fill preparation; map-only wall time is not separately measured.

Protected target quotas reuse exact leaf clipping and affine polygon moments.
Possibly intersecting finite beads are counted at their entire actual-prefix
amounts, deliberately overestimating local coverage and overlaps. Exact nominal
footprint clipping excludes disjoint/boundary-only records. These are one-sided
missing-volume witnesses; no whole-body union reintegration is required.
No geometric margin, precision, deadline or hard ceiling was relaxed. The
expected test-first missing-symbol build fails; both implementation builds and
all subsequent tests pass. Raw failure and successful logs remain archived.

Apple Clang 21 / macOS 26.5.1 ARM64 / Release builds OrcaSlicer and both test targets.
Material suites execute 48 cases / 12106 assertions; actual-body suites execute
13 cases / 107888 assertions, with NoAssertions. Selected CTest executes
292/292 without skips. Six fresh OFF/ZAA G-code/modal comparisons match pinned
stock with the established normalization and additive nptop_mode=off. Source
inventory and immutable package validation pass. Exact commands/status/exit,
XML, byte-preserved logs, source and binary hashes are under
`evidence/B07-material-deficit/`; original raw output is under
`build/nonplanar-evidence/B07-material-deficit/` and its separate baseline folder.
Large output/G-code is losslessly compressed in the committed archive.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialDeficit]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeMaterialDeficit]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-material-deficit/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-material-deficit-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-material-deficit-baselines --allow-nptop-off-default
```

Author review checks complete disjoint XY partition, immutable proof ownership,
continuous outer footprint enclosure, finite current butt/future exclusion,
exact binary amount sums, subset upper bounds, outward deficit subtraction and
summation, target conservation, and shared work/cancellation/deadline accounting.
Independent safety and whole-native-geometry review are pending. Complete B13
software provenance is pending; exact hashes identify this build despite the
retained 31461d50 configure label. The saved Linux snapshot confirms successful
4b3da47d run 36820170446; 84a709ef run 36826154741 is in progress. Neither
verifies this localization revision. Windows, physical qualification and
full-cycle performance are NOT_RUN.

Next: use the localized deficits when constructing useful finite paths, prove
that added material occupies actual remaining space, and recheck union/fill.
Finite-width/contact, perimeter/seam, later actual support, curved paths,
order/travel, full-head CCD, segment/flow limits, complete job/plate ownership
and independent final-byte replay/export remain open. The public guarded-hybrid
blocker is retained; the entire B01-B15 objective remains active.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-material-deficit.md`.
Diff: `git show <resolved-implementation-revision>`.
