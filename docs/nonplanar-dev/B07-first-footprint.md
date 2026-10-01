# B07 first-hatch amounts over the complete finite nominal roof domain

Invariant: an accepted first-hatch section-gap approximant bounds the continuous
highest laid nominal roof across the entire finite width, including its permitted
width departure. Its complete strip stays inside the owned D_lower-supported ROI.
ADR-0045 adds protected first-hatch contract 2 and distinguishes finite-width
results from retained centerline diagnostics. Original specification, fixtures,
goldens, profile/project/material/IR encodings and public export gating are unchanged.

Four analytical cases / 74 assertions pass. Both first axes reproduce independent
flat-roof rounded-section amounts. A finite path crossing rounded shoulders
encloses an independent monotone circular integral. An off-axis ridge inside the
width is refused although the centerline succeeds; the unlaid current remainder
is absent. Mutation preserves captured source/limits. Missing/index, work/packet/
depth exhaustion, unsupported outer-width ROI, cancellation, stale inner/outer
revision and changed rounding produce no snapshot. There is no sampled-only PASS.

One new actual native case / 18 assertions passes. The unchanged exact 1:16 wedge
is natively sliced with owned test-only 1.0 mm solid/top paths and zero-degree
infill. An independent section calculation selects an actual flat native bead
core containing the ROI. A 0.45 mm first hatch produces 61 constant-flux packets,
one roof leaf and 1911 roof evaluations. Its analytic amount is
0.12307872665804441 mm3. The case takes 0.168 s in the final body suite, including
preparation; planner-only time is unmeasured. This simulation does not alter or
qualify the user's U1 setup. Its policy retains maximum gap error 0.0001 mm, width
departure 0.002 mm, whole-line amount error 0.0001 mm3 and a five-second deadline.
Global work/segment/depth/numerical limits are unchanged.

Test-first compilation failed on the missing factory/domain (exit 1). Two
geometric runs had exit 42 from test assumptions: one flat roof genuinely needed
only one evaluation, and a zero boundary band was already invalid in the parent
hatch API. The work test now retains two real coincident records with the same
one-evaluation limit. The boundary test uses a valid positive 0.0001 mm band whose
outer width exceeds the supported ROI. All failures are archived. No negative
limit or safety margin was weakened. Final geometric/native/combined tests pass.

Apple Clang 21 / macOS 26.5.1 ARM64 / Release builds the app and both test targets.
Material suites execute 52 cases / 12180 assertions; actual-body suites execute
14 cases / 107906 assertions with NoAssertions. Selected CTest executes 297/297
without skips. Six fresh OFF/ZAA G-code and modal comparisons match pinned stock
with the existing timestamp/object normalization and additive nptop_mode=off
allowance. Source inventory and original package/checksums pass.

Exact argv, statuses/exit codes, actual XML and source/binary/raw/archive hashes
are in `evidence/B07-first-footprint/`. Raw logs remain under
`build/nonplanar-evidence/B07-first-footprint`; baseline capture is under
`build/nonplanar-evidence/B07-first-footprint-baselines`. Large outputs are
losslessly gzip-archived and verified after decompression.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[FirstHatchFootprint]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeFirstHatchFootprint]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-first-footprint/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-first-footprint-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-first-footprint-baselines --allow-nptop-off-default
```

Author audit covers exact width/ROI bounds, continuous roof enclosure, candidate
inheritance, current fraction, gap/width/amount/numeric budgets and provenance.
Independent safety review, Windows and physical execution remain NOT_RUN. The
saved Linux snapshot at 2026-10-01T07:29 UTC has 36828766748 at 7f454ba5 pending,
36826154741 at 84a709ef in progress and 36820170446 at 4b3da47d successful. These
earlier revisions do not verify this patch. Source/binary hashes bind the build;
complete B13 provenance is pending.

Rounded side-floor contact, target conformity, overlap/deficit repair, seam,
subsequent actual support, D_upper/head CCD, travel/order, motion/flow limits and
final-byte replay/export remain open. The complete B01-B15 objective is active.
Next construct paths in actual remaining space using the finite-domain proof,
then reconcile their occupied union and target filling. Deficit lower witnesses
alone are not extrusion quotas.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-first-footprint.md`.
Diff: `git show <resolved-implementation-revision>`.
