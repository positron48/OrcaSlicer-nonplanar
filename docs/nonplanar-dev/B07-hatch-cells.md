# B07 finite path-owned cells and complete boundary remainder

Invariant: finite hatch-owned rectangular cells and explicit end/boundary
remainders partition the entire parent pass exactly and preserve its bounded
nominal volume. Volume beyond a finite path's ends/nominal width cannot be
charged to that path. ADR-0039 records internal hatch contract 2 and new
factory-only hatch-cell contract 1; existing persisted formats are unchanged.

Three geometric cases / 254 assertions pass. Independent affine rectangle
integrals check every local cell, each finite interior and the complete remainder
on four alternating surfaces. Exact footprint containment, positive volume,
source lifetime and global precision are checked. A separate circular primitive
confirms that the original rounded bead's shoulders stay in the boundary
remainder while the finite first core remains above its flat nominal section.
Missing source, cell/work/precision exhaustion, staleness, cancellation and
deadline cannot publish. Callback changes to caller handles/limits do not alter
the owned source/request. The initial missing-API syntax failure (exit 1) is
archived; no negative case or production margin was relaxed.

One actual native case / 86 assertions passes. The unchanged exact 1:16 wedge
from ADR-0038 is captured, partitioned, sliced and reconstructed through native
Orca APIs. Its four-pass candidate layout is allocated using the actual first
roof proof and later prospective affine cells. Finite rectangular targets total
[5.88913621094819195,5.89412551467974843] mm3; the complete end/boundary remainder
is [3.76563665695931649,3.77063529532365616] mm3. The total is
[9.65477286790750888,9.66476081000340592] mm3, below 0.01 mm3 interval width and
consistent with the original complete pass-stack integral. Work is 3006 first
proof fragments/later cells and 10294 evaluations, within explicit limits
32768 cells / 200000 evaluations / 5 seconds. Each pass has one finite owner per
line and four remainder owners. Independent affine mean-height integrals check
all later finite interiors. The computed first roof volumes remain bounds from
the existing model, not physical or filled-bead qualification.

The app and both native test targets build with exit 0 on Apple Clang 21 / macOS
26.5.1 ARM64 / Release. Combined material suites execute 27 cases / 5205
assertions; actual-body suites execute ten cases / 54703 assertions, both with
NoAssertions. Selected CTest executes 268/268 without skips; exact commands are archived under
`evidence/B07-hatch-cells/`. Six fresh OFF/ZAA G-code/modal comparisons match the
pinned stock with the existing normalization and additive nptop_mode=off. Source
inventory and original package/checksums pass. Original fixtures/goldens,
configuration/build/dependency files and user presets are unchanged.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialModel],[MaterialCoverage],[MaterialTransition],[MaterialIntegral],[PassStack],[IntegralStrips],[AffineHatches],[HatchCells]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[BodyMaterial],[NativeCoverage],[NativeTransition],[NativeIntegral],[NativePassStack],[NativeIntegralStrips],[NativeAffineHatches],[NativeHatchCells]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-hatch-cells/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-hatch-cells-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-hatch-cells-baselines --allow-nptop-off-default
```

Author review checks private parent construction, exact finite containment,
contiguous/disjoint area ownership, complete end remainder, clipped proof
coverage, endpoint sums, total precision and callback/budget lifetime.
Independent safety review is pending. Native Linux CI 36805434167 at 12d8c81f
was pending and 36802647527 at 63779b4f in progress in the saved snapshot;
a083f2df's earlier run succeeded. This revision is not verified by those runs.
Windows and physical execution are NOT_RUN. Source/binary hashes bind the actual
build; the retained configure label 31461d50 is not full B13 software provenance.

Finite cell amounts are not selected rounded beads/E or evidence of dense fill.
Actual gap-dependent section/overlap corrections, adaptive constant-flux paths,
seam/perimeter construction, later deposited support, curved segmentation,
full-head/contact checks, legal order and final-byte replay/export remain open.
B07 and the complete B01-B15 objective are not complete. Next build nominal
finite bead amounts from these targets under the actual gap/section model.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-hatch-cells.md`.
Diff: `git show <resolved-implementation-revision>`.
