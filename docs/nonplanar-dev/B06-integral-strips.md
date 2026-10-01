# B06 exact strip volumes from an owned continuous roof proof

Invariant: split a previously bounded actual-roof integral into a complete exact
X/Y strip partition, preserving whole-domain nominal volume and global precision
without repeating material queries. ADR-0037 advances internal integral contract
to 2; existing persisted IR/ledger/body/profile formats are unchanged.

Two new geometric cases / 42 assertions pass. Independent circular primitives
confirm rounded-shoulder strip volumes, and an independent affine mean-height
integral confirms longitudinal allocation on a slope. Both axes, complete area,
positive volume and global precision are checked. Missing parent boundaries,
duplicate/reversed cuts, missing proof, exhausted work/cells/precision,
cancellation, staleness and deadline cannot publish. Callback mutation of the
caller proof/cuts/limits leaves the captured source and request intact.

One actual native case / 77 assertions passes. The unchanged 6x6 mm interior of
the reserved flat block is split into 12 half-millimeter strips along each axis.
Its nominal total remains [7.37352050100695777,7.3835184321566949] mm3, width
0.00999793114973713 mm3. Each total contains the independent native layer-cake
oracle [7.37753727144254245,7.37927018186442929] mm3 from the existing integral case.
Every local strip has positive volume; the complete nominal area is validated
exactly, not inferred from samples. Editing public diagnostic volume/source
fields cannot replace the protected proof. The source-bound selected first
surface from ADR-0036 also consumes the API: six added assertions verify its
12-strip allocation and precision from the same actual first-pass proof.

The first native run returned INTEGRAL_STRIP_CELL_LIMIT at 8191 fragments (exit
42). Retain that refusal as an explicit negative. Both axes pass with an explicit
32768-fragment allocation, producing 17620 X / 17614 Y fragments and 27252
leaf/strip evaluations per query. The footprint, material losses and 0.01 mm3
precision are unchanged. No D_nominal/upper/lower or Unknown semantics are
relaxed. The missing-API test-first syntax failure remains archived (exit 1).

The app and both native targets build with exit 0 on Apple Clang 21 / macOS
26.5.1 ARM64 / Release. Combined geometric suites execute 22 cases / 4688
assertions and actual-body suites eight cases / 54576 assertions with
NoAssertions. Selected CTest executes 261/261 without skips. Six fresh OFF/ZAA
G-code/modal comparisons match pinned stock with the existing normalization and
additive nptop_mode=off. The original package and checkout audit pass. Commands,
exit codes, XML, failure/native diagnostics and source/binary hashes are archived
under `evidence/B06-integral-strips/`; raw logs remain in
`build/nonplanar-evidence/`.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialModel],[MaterialCoverage],[MaterialTransition],[MaterialIntegral],[PassStack],[IntegralStrips]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[BodyMaterial],[NativeCoverage],[NativeTransition],[NativeIntegral],[NativePassStack],[NativeIntegralStrips]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B06-integral-strips/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B06-integral-strips-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B06-integral-strips-baselines --allow-nptop-off-default
```

Author review checks leaf preservation, protected ownership, exact closed
clipping/area, affine moments, separate endpoint sums, precision, domain/limit
refusals and callback lifetime. Independent safety review is pending. Native
Linux CI 36791501134 at b35408ce has succeeded; 36796275758 at a083f2df remains
in progress and 36800464013 at 3310bde1 pending in the saved snapshot. Those
results do not verify this revision. Windows and physical execution are NOT_RUN.

These are volume cells, not finite filled beads or E. Boundary/end/perimeter/seam
corrections, fixed-width path generation, actual subsequent support, full-head
contact/CCD, order and final-byte replay/export remain required. B06/B07 and the
entire B01–B15 goal remain open. Next use these local volume intervals to generate
finite rectilinear candidates with explicit width/endpoint and seam ownership.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B06-integral-strips.md`.
Diff: `git show <resolved-implementation-revision>`.
