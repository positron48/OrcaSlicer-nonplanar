# B07 adaptive fixed-width nominal bead amounts

Invariant: approximate one fixed nominal XY width with constant-flux G1 packets
under bounded whole-line volume error, using the existing material section law.
ADR-0040 adds internal fixed-width-bead contract 1 with a protected factory.
Existing material/IR/project/profile encodings and stock-mode behavior are unchanged.

Three geometric cases / 6400 assertions pass. Independent affine-gap section
integrals check rectangular and rounded target volumes. Every generated packet
is independently checked at endpoint/interior gap values for width bounds and
accepted by the existing material capture, which rechecks constant-flux volume,
section domain and midpoint nominal width. Ascending/descending gaps, reverse
motion, within-line endpoint continuity and the whole amount error are tested.
A constant-gap sloped line stays one packet and does not gain a 3D slope factor.
Missing/invalid context, unsupported section/width domain, zero XY length,
width/volume/depth/count exhaustion, staleness, cancellation, deadline and changed
rounding cannot publish. Callback changes to caller request/limits leave the
captured input intact. The test-first missing-API syntax failure (exit 1) is
archived. One later build failed on a wrong field name in the native test
(`material_ids` instead of `material`); the corrected build passes. No production
tolerance or negative case was weakened.

One actual native case / 122 assertions passes. Native capture/partition/slicing
and material reconstruction of the unchanged exact 1:16 wedge produce the owned
four-surface hatch context. For the three prospective later interfaces, 22 finite
lines produce 22 rounded packets with total commanded model amount
4.59539954527896288 mm3. Each line binds the actual body material fingerprint and
is accepted as a contiguous single-line ledger with the owned effective material
model. Independent affine mean-gap integrals check each target. The requested
width is 0.45 mm, maximum width departure 0.002 mm and whole-line volume error
0.00001 mm3. These later affine gaps are prospective; no actual first-roof affine
gap, deposited cap support, safe inter-line travel or legal full order is assumed.
The summed amount is not the measure of overlapping bead unions or a filled cap.

Apple Clang 21 / macOS 26.5.1 ARM64 / Release builds the app and both native test
targets with exit 0. Combined material suites execute 30 cases / 11605 assertions
and actual-body suites eleven cases / 54825 assertions with NoAssertions. Selected
CTest executes 272/272 without skips. Six fresh OFF/ZAA G-code/modal comparisons
match pinned stock with the existing normalization and additive nptop_mode=off.
Source inventory and original package/checksums pass. Exact commands, XML,
diagnostics and source/binary hashes are in `evidence/B07-fixed-width-beads/`;
raw logs remain in `build/nonplanar-evidence/`. Original fixtures/goldens,
build/dependency configuration and user presets are unchanged.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialModel],[MaterialCoverage],[MaterialTransition],[MaterialIntegral],[PassStack],[IntegralStrips],[AffineHatches],[HatchCells],[FixedWidthBeads]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[BodyMaterial],[NativeCoverage],[NativeTransition],[NativeIntegral],[NativePassStack],[NativeIntegralStrips],[NativeAffineHatches],[NativeHatchCells],[NativeFixedWidthBeads]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-fixed-width-beads/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-fixed-width-beads-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-fixed-width-beads-baselines --allow-nptop-off-default
```

Author review checks analytic XY volume, actual rounded binary64 amounts,
monotone width/domain bounds, shared dyadic endpoints, material-capture agreement,
fractional/global error budgets, declared source context and callback lifetime.
Independent safety review is pending. Linux CI 36807131177 at 2ee83d33 was pending;
36802647527 at 63779b4f remains live in its native-test build step in the saved
snapshot. Those runs do not verify this revision. Windows and physical execution
are NOT_RUN. Exact source/binary hashes bind this build; the configure label
31461d50 remains older and full B13 provenance is pending.

The complete B01-B15 objective remains open. Next reconstruct actual first-path
gaps and reconcile fixed-width rounded material with target-cell overlap,
perimeter/seam volumes; then prove subsequent deposited support, contact/CCD,
legal order, motion/E limits and final-byte replay/export. The existing public
safe-hybrid slice/export blocker remains.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-fixed-width-beads.md`.
Diff: `git show <resolved-implementation-revision>`.
