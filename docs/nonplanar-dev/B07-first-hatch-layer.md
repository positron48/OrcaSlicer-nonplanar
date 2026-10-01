# B07 complete first-hatch candidate construction and measurement

Invariant: construct every first source hatch against the same owned actual
body prefix under shared budgets, capture a fresh complete prospective ledger,
and measure its union before reconciling whole-cell fill. ADR-0049 specifies
the internal version-1 contract and exact congruent-stadium identity.

Two first-layer analytical cases / 145 assertions pass. Both axes retain all
source lines and independently enclose real stadium S/U/R/C rather than summed
coverage. Positive overlap and missing target remain visible. Source mutation,
clipped/mismatched domains, shared path/record/packet/roof/work/cell limits,
cancellation, stale revision and rounding refuse snapshots. A future off-axis
ridge or its current event before the true butt does not affect the candidate;
once present at the second line it refuses the entire candidate even though
the first line still constructs successfully.

One congruent-union case / 56 assertions covers both axes, reversed paths,
affine gaps/tips, aligned disjoint packets and actual missing longitudinal
space, with independent 113-bit amounts. Unequal profiles retain the general
solver and refuse the same two-cell / 1e-12 mm3 budget. The exact kernel path
requires two congruent axis-aligned rounded groups, contained complete nominal
material, matching actual amounts/tip/gaps and certified separation inside the
smallest flat core. It does not flatten sections or combine unequal packets.
Unsupported/partial/clipped/distant cases retain the existing integrator.

The unchanged native wedge and its actual sliced flat bead core now construct
both first lines / 122 packets together. The extended case passes 46 assertions
and independently encloses actual commanded S, U and R. Whole section-target
bounds enclose the ideal finite-section integral; actual commanded S differs
from that ideal by the retained global amount error and is measured separately.
Observed S is approximately 0.246158 mm3, C=U 0.138283 mm3, M 0.113705 mm3 and
R 0.107875 mm3, with 63 shared cells / 17802 work. Whole case elapsed time is
0.281 seconds locally, not a portable timing guarantee. Its old partial cap and
single remaining-path measurement remain separate; this fresh full candidate
is not appended to that partial prefix.

The initial native general integrator exhausted 65535 cells. Section-probe and
finite-butt partition trials also exhausted the same limit; all those kernel
changes are discarded. The retained exact identity succeeds without changing
amount, fill, cell, work, timeout or geometry limits. Raw failures/patches remain
diagnostic evidence, not certificates. Initial negative-test hypotheses about
future surface slope and equality of ideal versus commanded S were corrected
against the actual contracts; their failed outputs are retained.

Application and both native test targets build with Apple Clang 21 on macOS
26.5.1 ARM64, Release. Combined material suites pass 64 cases / 12742 assertions;
actual-body suites pass 14 cases / 107934 assertions. Selected CTest passes
309/309 without skips; six fresh OFF/ZAA G-code/modal comparisons pass against
pinned stock with existing normalization and additive nptop_mode=off allowance. Source
inventory and original package checks pass.

Linux run 36842571249 at 946d3da7 failed the unchanged native 20-second fill
limit: cells=40862/work=857455, provisional below [0.0150905,0.017343] mm3 and
above [0,0]. Run 36847680702 at 2d8bebaa is still building native tests in the
saved 11:06 UTC observation. Neither observation verifies this change. Linux
deadline repair remains unproved. Independent safety review, Windows and
physical qualification remain NOT_RUN. Public guarded export stays blocked;
full B01-B15 remains active, including complete remaining-space/global repair,
qualified contact/seam, curved/later actual support, head CCD and motion/order/
flow, job/plate/software provenance and final-byte replay/export.

Exact argv/status/exit, XML and raw/source/binary/archive hashes are in
`evidence/B07-first-hatch-layer/`; raw files remain in
`build/nonplanar-evidence/B07-first-hatch-layer` and fresh captures in
`build/nonplanar-evidence/B07-first-hatch-layer-baselines`. Gzip archives preserve
original bytes after decompression. Original specification, fixtures, goldens,
profiles and build/dependency settings are unchanged.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[FirstHatchLayer],[CongruentUnion]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeFirstHatchLayer]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-first-hatch-layer/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-first-hatch-layer-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-first-hatch-layer-baselines --allow-nptop-off-default
```

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-first-hatch-layer.md`.
Diff: `git show <resolved-implementation-revision>`.
