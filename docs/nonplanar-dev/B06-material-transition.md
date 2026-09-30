# B06 first-pass gap and volume bounds from actual material

Invariant: an affine first-pass cell can be Compatible only with whole-footprint
D_lower support and a complete gap interval bounded against D_upper from the
same owned deposited prefix. ADR-0034 defines internal contract 1.

Four new geometric cases / 55 assertions pass. They cover actual stepped rows,
independent step-volume integration, partial current geometry, varying height
and gap, transverse growth, and a tall diagonal neighbor whose bounding box
overlaps the low footprint. A real bead over the footprint blocks. Unlaid future
material, missing support, uncertain gap, work exhaustion, stale/cancelled/late
queries, invalid domain/context and changed rounding all block. Callback mutation
of the caller's handle, cell and policy cannot change the captured result.

One new native case / 29 assertions passes on the complete actual reserved-body
material from Orca. The same 0.01 mm inner XY/Z losses and inherited/native flow
and coordinate error remain charged. The [17,23]x[17,23] footprint at Z=1.9 is
continuously supported. A proposed affine surface at Z=2.2 has its complete gap
strictly inside the illustrative [0.1,0.32] mm limits. Real walls reaching Z=4
elsewhere do not become an imaginary roof inside the cap. A support plane at
Z=2.5 rejects, a proposed surface at Z=2.6 rejects, and extending into the cap
edge is blocked. These are software fixtures, not U1 process parameters.

Nominal volume is bounded by the affine integral above the highest nominal roof,
without adding commanded bead sums or double counting overlaps. An independent
rectangular-step integral is contained in the geometric result. For native
rounded infill the existing independent Clipper support-plane oracle and the
native roof height imply a deliberately broad 7.2 to 10.8 mm3 interval (with
outward rounding). The exact corrugated nominal volume is not solved or selected;
no deposition event/E is produced from the interval. Full B06 is unfinished.

The combined geometric suites execute 14 cases / 4555 assertions and actual-body
suites five cases / 46163 assertions with NoAssertions. Initial test-first syntax
compilation fails with the missing API (exit 1); its diagnostic is retained.
Two application/test incremental builds pass, using the already corrected pinned
GMP Apple ARM64 ABI. Final selected CTest executes 250/250 without disabled or
skipped cases. Six fresh OFF/ZAA G-code/modal comparisons match pinned stock,
allowing only existing normalization and additive nptop_mode=off. Apple Clang 21,
macOS 26.5.1 ARM64, Release; exact commands/statuses/XML/source and binary hashes
are under `evidence/B06-material-transition/`. Original goldens/bundle are unchanged.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialModel],[MaterialCoverage],[MaterialTransition]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[BodyMaterial],[NativeCoverage],[NativeTransition]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B06-material-transition/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B06-material-transition-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B06-material-transition-baselines --allow-nptop-off-default
```

Author review checked the outer rectangle/stadium enclosure against membership,
exact clipping, partial-prefix parameter clamping, affine extrema/integral,
separate upper/lower uses, shared work/deadline and captured callback ownership.
This is not independent review. There is no new serialized field/cache; existing
ledger/body hash vectors pass unchanged. New-revision Linux is NOT_VERIFIED and
Windows/physical execution are NOT_RUN. The saved prior Linux snapshot confirms
e7a46961/23f00bfd success and b35408ce running; it does not verify this patch.

Next: refine the nominal roof integral and subdivide actual stepped support into
qualified first-pass cells, select legal thickness/volume, and solve the volume
at seam/edge and later surfaces. Complete cap paths, contact/full-head CCD,
legal ordering, independent final-byte replay and unified export gating plus
remaining source/plate/job/worker boundaries. Full B01-B15 stays active. Guarded
export remains blocked; no profile writes, printer connection or physical trial.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B06-material-transition.md`.
Diff: `git show <resolved-implementation-revision>`.
