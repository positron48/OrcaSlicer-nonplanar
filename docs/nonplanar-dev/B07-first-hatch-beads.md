# B07 first hatch amounts from the actual nominal roof

Invariant: derive a first-pass centerline's continuous nominal gap from the
owned laid-material prefix, and bound its constant-flux width and whole-line
amount against that gap. ADR-0041 adds protected internal first-hatch contract 1.
Existing material/IR/project/profile encodings and original fixtures are unchanged.

Three geometric cases / 87 assertions pass. A flat nominal roof produces the
independent analytic rounded-section amount, despite a different lower support
plane. A path crossing rounded shoulders encloses an independent monotone
4096-interval circular integral. Every interval covers its whole segment;
endpoint samples cannot accept production candidates. A higher future bead is
excluded. Caller mutations during callbacks preserve captured source/limits.
Missing/index/domain, query/roof/packet/volume exhaustion, cancellation, stale
outer or inner revision, deadline and changed rounding publish no snapshot.
The initial missing-API test-first compile failure is archived. One build failed
on a mixed-type C++ auto declaration; separating its declarations corrected it.
No geometry margin or negative test was weakened.

One actual native case / 52999 assertions passes. The unchanged exact 1:16 wedge
is captured, partitioned, natively sliced and reconstructed as an owned material
prefix. All 14 first hatches reconstruct its rounded roof. They produce 17624
constant-flux packets / 17374 roof leaves, with 128128 section evaluations across
the lines and total commanded model amount 1.39559549694415286 mm3. Substituting
the lower support plane instead would give 1.91227202443585464 mm3. This is a
summed amount, not the measure of overlapping bead unions or a filled cap.

The native fixture requests gap error 0.0001 mm, width departure 0.002 mm and
whole-line amount error 0.0001 mm3, with a five-second cooperative deadline.
Observed maxima: gap error 0.0000999912920942591925 mm, width departure
0.00199218936355261044 mm and line amount error 0.0000223681970210054571 mm3.
An independent long-double point section calculation checks packet interior
gaps/widths; these samples are QA, not the continuous certificate or an independent
whole-native-volume oracle. Each contiguous line is accepted by the existing
material capture with the reported combined approximation/numeric error charged
to its declared model. Affine section fields remain approximants; real finite-width
floor/contact, pressure continuity and subsequent cap support are not established.

Apple Clang 21 / macOS 26.5.1 ARM64 / Release builds the app and both test targets.
Combined material suites execute 33 cases / 11692 assertions; actual-body suites
12 cases / 107824 assertions with NoAssertions. Selected CTest executes 276/276
without skips. Six fresh OFF/ZAA G-code and modal comparisons match pinned stock
with existing normalization/additive nptop_mode=off. Original package/checksums
and source inventory pass. Exact commands, XML, diagnostics and hashes are under
`evidence/B07-first-hatch-beads/`; raw files remain in `build/nonplanar-evidence/`.
The verbose native diagnostic log is losslessly gzip-archived with its raw hash.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialModel],[MaterialCoverage],[MaterialTransition],[MaterialIntegral],[PassStack],[IntegralStrips],[AffineHatches],[HatchCells],[FixedWidthBeads],[FirstHatchBeads]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[BodyMaterial],[NativeCoverage],[NativeTransition],[NativeIntegral],[NativePassStack],[NativeIntegralStrips],[NativeAffineHatches],[NativeHatchCells],[NativeFixedWidthBeads],[NativeFirstHatchBeads]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-first-hatch-beads/ctest-final --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-first-hatch-beads-final-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-first-hatch-beads-final-baselines --allow-nptop-off-default
```

Author review checks the exact prefix/current fraction, continuous roof bounds,
Z/gap correlation, XY-only volume, stored binary64 constant area, actual-gap
width/domain bounds, fractional/global amount budgets, source lifetime and
combined numeric accounting. Independent safety review is pending. The saved
final Linux snapshot has run 36802647527 at 63779b4f successful, including native
tests and application, and run 36809462993 at 8bcbbd45 in progress. These earlier
revisions do not verify this patch. Windows and physical execution
are NOT_RUN. Exact source/binary hashes bind this build; the older configure label
31461d50 remains and complete B13 provenance is pending.

The tight native policy generates many short segments: motion/flow/firmware
frequency limits remain unverified. Whole finite-width material/contact,
perimeter/seam overlap and filled-volume reconciliation, legal travel/order,
subsequent actual deposited support, full-head CCD and final-byte replay/export
remain required. The public safe-hybrid slice/export blocker remains. The entire
B01-B15 objective is still active.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-first-hatch-beads.md`.
Diff: `git show <resolved-implementation-revision>`.
