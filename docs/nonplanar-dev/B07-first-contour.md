# B07 first closed ROI contour

Invariant: own four connected finite-width edges on the original first affine
surface, preserving actual body/target and original boundary band. Prove every
whole footprint and measure the joint S/U/R/C/M/spill, including corner overlap,
before publishing a complete loop. Explicit starting corner and CW/CCW traversal
do not qualify joining material or corner motion. ADR-0052 records the internal
owner migration and qualified one-dimensional union domains.

Two contour cases / 558 assertions pass. Eight seam/direction variants close
exactly in XYZ, retain immutable original source and have no false infill owner.
Independent 113-bit stadium arithmetic encloses commanded amounts and merged
loop union/excess. Connected edges create no zero-length travel. A ridge first
reached by a later edge refuses the whole loop; future material remains absent
and actual current-prefix changes are respected. Mutable wrappers, invalid
policy/width/ROI/box, shared budgets, deadline, revision, cancellation and rounding
reject as appropriate.

Two union-oracle cases / 124 assertions pass. Independent circular section
moments enclose merged, unmerged and partially merged constant loops on both
axes. Independent circle/line spill encloses a sloped two-packet loop, including
reverse traversal. Real gaps, unequal sections, partial current events and
clipped boxes cannot use a full-loop certificate. Qualified exact/one-dimensional
kernels retain actual amounts and rounded sections with original work/cell/depth,
precision and deadline limits; unsupported layouts retain the general solver.

The unchanged native wedge and actual sliced flat bead core pass the extended
360-assertion case. The separate contour has four edges / 108 packets at declared
0.4 mm path width. S is approximately 0.237329 mm3, C is
[0.149216,0.149627] mm3, M is [0.102361,0.102772] mm3 and outside volume is
[0.0000172428,0.000213198] mm3. Real rounded end spill remains positive and
bounded by the declared 0.001 mm3 limit. Independent section integrals enclose
each native edge amount. Joint measurement uses 145 shared cells / 30635 work.
The standalone whole case took 0.414 seconds; the final actual-body run records
0.371 seconds inside Catch. These are local observations, not portable guarantees.
Global amount error 1e-4 mm3, gap error 1e-4 mm, width error 0.002 mm, fill precision
0.001 mm3, 65535 cells / 2M work and five-second root limit remain unchanged.

The contour is not yet combined with the prior end/width-replanned infill.
Its deficit exceeds that separate candidate's approximately 0.0709876 mm3;
no coverage improvement over infill is claimed. Common measurement now charges
actual path/piece summation: the unchanged width candidate uses 18119 work
(historical width report: 17985), first layer 17926 and end replan 18474.
Native material values remain consistent with the prior stages.

Test-first missing-API and test-only Catch-parenthesis build failures are
retained. General-grid trials exhausted 4095 analytical cells and 65535 native
cells before specialized qualified integration; no fixture, precision or budget
was weakened. First-bead contract 4 -> 5 makes original hatch ownership optional
for protected contour edges; new first-contour contract is 1. All consumers are
rebuilt. No persisted cache/IR/profile/3MF representation or public default changes.

Application and both test targets build with Apple Clang 21, macOS 26.5.1 ARM64,
Release. Final material suites pass 74 cases / 13886 assertions and actual-body
suites pass 14 cases / 108248 assertions. Selected CTest passes 319/319 without
skips; six fresh OFF/ZAA G-code/modal comparisons pass against pinned stock
with existing normalization and additive nptop_mode=off allowance. Source inventory
and original package validation pass. Normative bundle, models, goldens, profiles,
CMake and dependencies remain unchanged.

Linux runs 36847680702 at 2d8bebaa, 36853913690 at 361ccdc8 and 36858083109 at
05f4a887 succeed in selected native suites, STL CLI and application. The saved
13:47 UTC snapshot shows run 36861891679 at 4e6cddd7 building native tests.
This new contour awaits its own Linux verification. The original 20-second
native-fill timeout remains unchanged. Windows, independent safety review and
physical qualification remain NOT_RUN.

Full B01-B15 remains active and guarded export remains BLOCK. Joining material,
contact, full-head access, order/motion/flow, remaining 3D repair, actual partial
prefixes and curved/later support remain required. Job/plate/software binding
and independent final-byte replay remain pending. Author numerical checks and
source audits are not independent safety review. Standard U1 head and nominal
0.4 mm nozzle remain user declarations, without physical qualification claims.

Exact argv/status/exits, XML, raw/source/binary/archive hashes are retained in
`evidence/B07-first-contour/`; raw outputs are in
`build/nonplanar-evidence/B07-first-contour`, with fresh captures in
`build/nonplanar-evidence/B07-first-contour-baselines`. Gzip files preserve the
original bytes after decompression.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[FirstContour],[ClosedLoopUnion],[LoftLoopUnion]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeFirstContour]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-first-contour/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-first-contour-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-first-contour-baselines --allow-nptop-off-default
```

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-first-contour.md`.
Diff: `git show <resolved-implementation-revision>`.
