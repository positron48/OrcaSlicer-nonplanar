# B06 refined nominal first-pass volume over the actual bead union

Invariant: Bounded encloses the integral above the highest nominal deposited roof
and meets an explicit whole-footprint interval width. Continuous first-pass gap
and D_lower support from the same prefix are prerequisites. ADR-0035 defines
internal integral contract 1. No point samples authorize the integral.

Three new geometric cases / 37 assertions pass. An independent circular
antiderivative confirms rounded shoulders, an independent rectangular-step
integral confirms highest-roof overlap handling, and a diagonal affine case
confirms the exact polygon moment/cap integral. Limits, depth, zero tolerance,
stale/cancelled/late work, callback mutation, missing support and unlaid future
material preserve blocking behavior. No arbitrary volume midpoint is selected.

One new actual native case / 8256 assertions passes. The complete 6x6 mm cap
interior retains the same 0.01 mm inner XY/Z losses and all inherited numerical
error. At the illustrative Z=2.2 first surface / Z=1.9 support plane, the refined
nominal volume interval is [7.37352050100695688,7.38351843215669756] mm3, width
0.00999793114974068 mm3. This replaces the prior diagnostic [7.2,10.8] bounds
without using summed commanded bead volumes or hiding overlapping material.
The query uses 4541 integral cells / 11066 integral candidate evaluations; combined
support/roof work remains within the declared 8191 cells / 200000 evaluations /
5 seconds. These are software fixture parameters, not U1 process settings.

An independent native oracle constructs 64-micron-inset/expanded section
rectangles before 1-micron integer quantization, unions them with Clipper, and
uses monotone upper-half cross-sections at 2000 Z intervals. Lower/upper Riemann
bounds enclose the continuous layer-cake integral between planes, rather than
accepting sampled material or reusing the planner roof function. Its separate
1e-7 mm3 floating-sum allowance and actual per-row Z are retained. The oracle
interval [7.37753727144254245,7.37927018186442929] mm3 is fully contained in the
planner interval. Its domain checks exclude higher walls continuously in XY,
prove the whole base plane covered, and ensure monotone upper-half sections.
This independent oracle applies to the horizontal native fixture, not arbitrary
curved beads/jobs. Apple ARM64 long double has binary64 precision; the 64-micron
geometry inset dominates arithmetic/quantization error in this small fixture.

Initial native work returned Unknown at the unchanged 4095-cell default. The
explicit larger bounded work allocation retains the same footprint and 0.01 mm3
precision. A subsequent oracle test found an incorrect assumption of exactly
Z=2 in every native row: some actual values are 1.99999999999999978. The oracle
now uses each original binary64 top and their true maximum without normalization.
Both exit-42 failures and the missing-API test-first exit-1 diagnostic remain.
There was no production Unknown-to-PASS conversion or geometry margin reduction.

Combined geometric suites pass 17 cases / 4592 assertions and actual-body suites
six cases / 54419 assertions with NoAssertions. The app and both targets build
with exit 0 on Apple Clang 21 / macOS 26.5.1 ARM64 / Release, using the corrected
pinned GMP ABI. Final selected CTest executes 254/254 without disabled/skipped
cases; six fresh OFF/ZAA G-code/modal comparisons match pinned stock, with only
existing normalization and additive nptop_mode=off. Exact commands, exit status,
XML, failures, diagnostic values, source and binary hashes are archived under
`evidence/B06-material-integral/`; full raw logs stay in `build/nonplanar-evidence/`.
The unchanged bundle validates; existing canonical ledger/body vectors still pass.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialModel],[MaterialCoverage],[MaterialTransition],[MaterialIntegral]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[BodyMaterial],[NativeCoverage],[NativeTransition],[NativeIntegral]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B06-material-integral/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B06-material-integral-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B06-material-integral-baselines --allow-nptop-off-default
```

Author review checked maximum-roof semantics, whole-cell lower bounds, nonlinear
rounded sections, exact child-domain preservation and moment integrals, separate
endpoint sums, candidate exclusion, global precision and total work/deadline,
callback lifetime and revision/rounding. Independent safety review is pending.
New-revision Linux is NOT_VERIFIED; Windows and physical execution are NOT_RUN.
This query does not cover complete seam/perimeter allocation, arbitrary curved
surfaces or selected E, and is not the complete B06 solver or guarded export.

Next use these refined bounds to select qualified first-pass cells/thickness and
path volume, solve the volumetric seam/edge and subsequent surfaces, then cap
paths, contact/full-head CCD, ordering and final-byte replay/export. Full B01-B15
stays active with remaining source/plate/job/worker boundaries. No printer action
or user profile/config write occurs.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B06-material-integral.md`.
Diff: `git show <resolved-implementation-revision>`.
