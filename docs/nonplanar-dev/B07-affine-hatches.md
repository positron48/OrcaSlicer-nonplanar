# B07 finite affine hatch candidates

Invariant: owned finite fixed-width X/Y centerline alternatives stay on the
selected affine surfaces, fit wholly inside their nominal parent ROI and retain
complete prospective strip volume. ADR-0038 advances internal pass-stack contract
to 2 and introduces affine-hatch contract 1. Existing persisted formats and
stock-mode behavior are unchanged.

Two geometric cases / 263 assertions pass. Independent affine interpolation
checks endpoint Z, and an independent rectangular-source mean-height integral
checks the total volume for both first-axis choices. Tests check fixed width,
alternating directions, actual pitch bounds, positive lengths/local volume,
finite capsule extents and reverse alternatives. Missing input, zero band,
unsupported direction, sparse spacing, thin ROI, work/line exhaustion,
cancellation, stale revision and deadline cannot publish. Mutating caller
handles, policy and limits from a callback retains the captured request.

One actual native case / 41 assertions passes. A separate twelve-triangle wedge
has 32x20 mm footprint, top Z from 4 to 6 mm, slope 1:16 and exact volume 3200 mm3.
At native instance offset (20,20,0), the independent final-height oracle is
`F(x,y)=5+(x-20)/16`. A 2x6 mm interior ROI above the actual reserved/sliced body
produces four alternating passes and 36 finite centerline candidates. Width is
0.45 mm, maximum XY pitch 0.4 mm and configured outer band 0.2 mm. First-pass
volume comes from the actual owned nominal-roof proof; later volumes are
prospective affine slabs. Their total is
[9.65477286790751243,9.6647608100034077] mm3, width below 0.01 mm3. This is a
computed cell-volume bound, not an independently qualified filled-bead amount.
Final endpoint Z matches the analytic source; AlongX lines change Z. Native
cancellation, outer deadline, staleness, line limit and caller-parent reset are
also exercised. The new fixture does not replace the original wedge or goldens.

Both missing-API test-first syntax failures are retained (exit 1). The first
geometric execution failed one assertion (exit 42): the test incorrectly required
AlongY motion to change Z on an X-only slope. The corrected oracle checks AlongX
variation and different heights between AlongY rows. No production tolerance or
negative case was weakened. A diagnostic generic-binary run explicitly reported
no matching native tag; it is not counted as native evidence. The native case
was then executed in fff_print_tests. Two initial audit commands used the wrong
script directory (exit 2); corrected documentation scripts both pass.

Apple Clang 21 / macOS 26.5.1 ARM64 / Release builds the app and both test targets
with exit 0. Combined material suites execute 24 cases / 4951 assertions and
actual-body suites nine cases / 54617 assertions with NoAssertions. Selected
CTest executes 264/264 without skips; results and exact commands are archived in
`evidence/B07-affine-hatches/`.
Six fresh OFF/ZAA G-code/modal comparisons match pinned stock with the existing
normalization and additive nptop_mode=off. The original package/checksum audit and
checkout audit pass. Raw logs remain in `build/nonplanar-evidence/`.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialModel],[MaterialCoverage],[MaterialTransition],[MaterialIntegral],[PassStack],[IntegralStrips],[AffineHatches]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[BodyMaterial],[NativeCoverage],[NativeTransition],[NativeIntegral],[NativePassStack],[NativeIntegralStrips],[NativeAffineHatches]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-affine-hatches/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-affine-hatches-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-affine-hatches-baselines --allow-nptop-off-default
```

Author review covers protected parent construction, input/callback ownership,
outward coordinate errors, stored pitch, whole nominal capsules, exact later
moments, separate endpoint sums and all deadline/revision bridges. Independent
safety review remains pending. Linux CI 36796275758 at a083f2df succeeded;
36802647527 at 63779b4f was in progress in the saved snapshot. Those commits do
not verify this revision. Windows and physical execution are NOT_RUN.

These candidates do not yet allocate nominal finite-bead volume or E, complete
boundary/end/perimeter/seam geometry, actual later support, curved segmentation,
full-head/contact checks, travel or legal order. The existing safe-hybrid
slice/export blocker remains. B07 and the whole B01-B15 objective remain open.
Next construct finite bead and seam allocations from these owned volume targets.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-affine-hatches.md`.
Diff: `git show <resolved-implementation-revision>`.
