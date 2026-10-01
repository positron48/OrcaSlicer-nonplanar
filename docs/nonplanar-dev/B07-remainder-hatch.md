# B07 constructive remaining first-hatch paths

Invariant: construct XY-disjoint remaining intervals of a first hatch, prove
their complete finite nominal roof/amount bounds and measure a positive target
fill gain under the outside-volume limit. ADR-0046 defines the prospective result
and preserves the exact old current prefix separately from the added ledger.

Three analytical cases / 178 assertions pass. Both axes construct full/unlaid
suffixes at current progress 0, 0.3 and 0.5. Independent rounded-section amounts
enclose the measured added coverage. A completed middle obstacle leaves two
separated candidates with shared packet/path limits. An off-axis D_upper neighbour
blocks even though its nominal centerline is clear. Full occupancy, wrong owned
target/body, insufficient gain, real transverse target spill, invalid/exhausted
budgets, stale callbacks, cancellation and changed rounding publish no snapshot.
Caller mutation retains captured inputs/limits. Original future material is absent;
replacement paths cannot be appended beside its original future events.

The existing native flat-core case now has 29 assertions. It retains the actual
1:16 wedge and native body, reconstructs a partially laid 61-packet first hatch
at record 30 plus progress 0.3, then constructs one finite remaining path. The
measured added target coverage is at least 0.06858454570256423 mm3. Prospective
combined coverage is [0.12052052173081913,0.12122258415834322] mm3 and remaining
deficit [0.13076540743244636,0.13146746985997773] mm3. Deficit remains positive.
No lower-witness quota is blindly converted to E. Native settings remain test-only;
the user's U1 configuration/presets are unchanged.

Default policy: 0.01 mm extra separation from the existing D_upper projection,
at least 0.001 mm3 added covered volume and at most 0.001 mm3 total outside volume
inside the owned box. Default component precision is 0.001 mm3; final interval
width is at most 0.01 mm3. Global limits are 64 paths, 65535 cells and two million
work units; nested first-bead limits remain unchanged and are shared across paths.
Source walks, clipping/sort comparisons and reported kernel work are charged.
The outer five-second and nested cooperative deadlines are captured together.

Test-first compilation failed on the absent API (exit 1); subsequent builds/tests
pass. The app and both test targets build with Apple Clang 21 / macOS 26.5.1
ARM64 / Release. Final material suites execute 55 cases / 12358 assertions;
actual-body suites execute 14 cases / 107917 assertions with NoAssertions.
Selected CTest executes 300/300 without skips. Six fresh OFF/ZAA G-code/modal
comparisons pass against pinned stock with the existing normalization and additive
nptop_mode=off allowance. Source inventory and original package/checksums pass.

Exact argv, completed statuses/exit codes, actual XML and source/binary/raw/archive
hashes are in `evidence/B07-remainder-hatch/`. Raw logs remain under
`build/nonplanar-evidence/B07-remainder-hatch`; captures are under
`build/nonplanar-evidence/B07-remainder-hatch-baselines`. Large outputs are verified
after lossless gzip decompression. Original package, fixtures and goldens are unchanged.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[RemainderHatch]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeRemainderHatch]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-remainder-hatch/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-remainder-hatch-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-remainder-hatch-baselines --allow-nptop-off-default
```

Saved Linux snapshot at 2026-10-01T08:36 UTC: run 36831523726 at 7f71cc98 is
in progress; run 36828766748 at 7f454ba5 was cancelled; run 36826154741 at 84a709ef
failed its native CTest gate. Its check annotation records MATERIAL_DEADLINE in
the existing fill case at 41017 cells / 865782 work, with below volume bounded
and above interval still [0,0.00421376]. The 20-second limit stays unchanged.
Full job-log download returned HTTP 403; the authoritative failure annotation,
job metadata and artifact listing are archived. Current Linux verification remains
pending. Earlier 4b3da47d run 36820170446 passed.

Author audit checks exact blocked/current intervals, inward endpoints, protected
source/target and policy capture, finite width, shared budgets, fresh ledger
validation, measured fill and disjoint-set addition. Independent safety review,
Windows and physical execution are NOT_RUN. Rounded side-floor contact, full
3D remainder/global replanning, complete seam/edges, later support, head/travel/
order/flow and final-byte replay/export remain open. Public export stays blocked;
full B01-B15 remains active. Next fix the inherited Linux deadline through valid
geometric bound tightening, then continue complete path construction.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-remainder-hatch.md`.
Diff: `git show <resolved-implementation-revision>`.
