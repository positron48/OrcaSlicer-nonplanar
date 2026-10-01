# B07 finite nominal occupied union

Invariant: measure the clipped union of actual-prefix declared beads separately
from summed commanded amount, with continuous bounds for multiplicity excess.
ADR-0042 introduces protected internal material-union contract 1. Existing
serialized material/IR/project/profile contracts and original fixtures are unchanged.

Seven geometric cases / 170 assertions pass. Independent commanded-section,
circular-lens and affine trapezoid integrals check coincident and neighboring
rounded/rectangular beads, XYZ clipping, XY-only slope convention, rotated and
reversed paths, the current fraction, future exclusion and finite missing-packet
voids. Triple coincident occupancy checks R=2V rather than pairwise 3V. Callback
mutation preserves owned source/domain/limits. Missing source, cell/work
exhaustion, stale revision, cancellation, timeout and changed rounding publish
no snapshot. A clipped shoulder domain requires real refinement for the negative
cell-limit test; a fully contained single bead has an exact analytic amount.

One actual native case / 37 assertions passes. The unchanged exact 1:16 wedge is
captured, partitioned, natively sliced and reconstructed. Its 14 first hatches
produce 2109 packets using the published 0.001 mm gap and 0.001 mm3 line-amount
defaults, with 0.002 mm width departure. The existing tighter first-hatch test is
retained unchanged. Declared connector travels do not approve a motion order.

The physical integration domain is [19,21] x [17,23] x [4.0,4.7] mm. Independent
113-bit addition of the binary packet amounts gives S=1.39569413683378021 mm3.
The certified occupied interval is [1.31221597619643471,1.32221578176234611] mm3;
the multiplicity-excess interval is [0.0734783550714341455,0.0834781606373454221]
mm3. All three interval widths meet the requested 0.01 mm3. This run uses 32213
evaluated cells / 1598991 work units and takes 8.227 seconds including fixture
preparation, within the unchanged ten-second union deadline. No target cap-fill
or physical contact claim follows from these declared model measures.

Test-first missing-API compilation fails as expected. Retained failed runs exposed
slow convergence and work/cell/deadline exhaustion. Exact nominal boxes, immutable
coefficient caching, slope shear, finite nonoverlapping chain counts, correlated
rounded-height bounds and concavity integration resolve these without increasing
the native tolerance, deadline or hard limits. One compile error in an array
initializer was corrected. One diagnostic run after that failed build used the
prior binary and is not final-source evidence. The final native summation check
initially failed because Apple ARM64 long double is binary64; 113-bit reference
addition now avoids that accumulation error. All failure logs and provisional
diagnostics remain archived, without promotion to accepted certificates.

Apple Clang 21 / macOS 26.5.1 ARM64 / Release builds OrcaSlicer and both test targets.
Combined material suites execute 40 cases / 11862 assertions; actual-body suites
13 cases / 107861 assertions with NoAssertions. Selected CTest executes 284/284
without skips. Six fresh OFF/ZAA G-code and modal comparisons match pinned stock,
using existing normalization and additive nptop_mode=off. Source inventory and
immutable package validation pass. Exact commands, exit codes, XML and hashes
are under `evidence/B07-material-union/`; raw files remain under
`build/nonplanar-evidence/B07-material-union/` and the separate baseline directory.
Raw logs are byte-preserved as .txt in the committed archive.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialUnion]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeMaterialUnion]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-material-union/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-material-union-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-material-union-baselines --allow-nptop-off-default
```

Author review checks actual prefix/fraction ownership, finite butt boundaries,
chain coloring and gap coverage, monotone profile-count bounds, correlated
rounded depth, positive-overlap concavity preconditions, physical Z clipping
under shear, exact amount sums, arithmetic environment and all budgets.
Independent safety review is pending. The native union lacks an independent
whole-native-union oracle; independent analytical section examples test the
algorithm, and the amount oracle only checks S. Complete B13 software provenance
is pending; exact source/binary hashes bind this build despite the older 31461d50
configure label. Linux run 36809462993 at 8bcbbd45 now succeeds, including native
tests and application. Run 36812147275 at e2030e15 is in progress in the saved
snapshot. These earlier revisions do not verify this union patch. Windows,
physical qualification and full-cycle performance are NOT_RUN.

Next: compare finite occupied geometry with the target cells, localize underfill
and excess, qualify finite-width floor/contact, then perimeter/seam and actual
subsequent support. Curved paths, legal order/travel, full-head CCD, segment/flow
limits, job/plate binding and independent final-byte replay/export remain open.
The public safe-hybrid blocker remains; the entire B01-B15 objective stays active.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-material-union.md`.
Diff: `git show <resolved-implementation-revision>`.
