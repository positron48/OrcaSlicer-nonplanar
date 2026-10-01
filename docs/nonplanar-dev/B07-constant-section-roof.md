# B07 continuous interior constant-section roof queries

Invariant: evaluate the complete nominal roof of a constant-Z/gap bead directly
from its normal projection when the whole query polygon is strictly between its
finite butts. ADR-0048 states the proof and the cases retaining exact XY clipping.
Actual rounded shoulders, width from commanded amount, current progress and
all existing limits are preserved.

Four new analytical cases / 157 assertions pass. Independent circle-segment
integrals verify real below-body material at current progress 0.3 and completion,
with both loose and tighter fill precision. A raised cap retains genuine above-
target volume. Future raised body rows remain absent. Reverse diagonal partial
body support with both cap axes matches independent flat-core/prism measures.
A higher rectangular neighbour outside the footprint contributes no roof.
Mutation retains captured sources; cancellation, stale revision, changed
rounding and exhausted shared work/cell budgets publish no snapshot.

The application and both native test targets build with Apple Clang 21,
macOS 26.5.1 ARM64, Release. Combined material suites execute 61 cases / 12541
assertions; actual-body suites execute 14 cases / 107917 assertions. Selected
CTest executes 306/306 without skips. Six fresh OFF/ZAA G-code/modal comparisons
pass against pinned stock with existing normalization and additive nptop_mode=off
allowance. Source inventory/package checks pass. The existing
rounded native case preserves 40960 fill cells / 865083 work and below interval
[0.0150916,0.0173416] mm3, with above upper bound 1.17968e-14 mm3. Its separately
measured union/fill times are 6.99115 / 16.01 seconds. Whole case time is 24.882
seconds, compared with the prior locally recorded 26.573 seconds. These are
observations on this machine, not portable timing guarantees.

Earlier tile-index trials failed the native work limit and are excluded from
the implementation. Their raw logs/XML and the final discarded source diff are
retained for diagnosis. The existing Linux deadline failures at 84a709ef and
7f71cc98 remain historical red evidence; correctness tests were added before
this optimization. Final source/binary hashes identify the direct section path,
not the discarded index.

Saved Linux observation at 2026-10-01T10:05 UTC: run 36842571249 at 946d3da7 is
in progress, building native tests. It does not execute this change. Linux
deadline repair remains pending; the 20-second fill timeout is unchanged.
Windows, independent safety review and physical execution are NOT_RUN. Public
guarded export stays blocked and the full B01-B15 objective remains active.

Raw outputs retain their original trial directory
`build/nonplanar-evidence/B07-roof-tile-fill`. Fresh captures are under
`build/nonplanar-evidence/B07-constant-section-roof-baselines`. Exact argv,
completed statuses/exit codes, XML and source/binary/raw/archive hashes are in
`evidence/B07-constant-section-roof/`. Gzip outputs retain exact bytes after
decompression. The original specification package, fixtures and goldens are unchanged.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[FlatRoofQueries]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-roof-tile-fill/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-constant-section-roof-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-constant-section-roof-baselines --allow-nptop-off-default
```

Next verify the unchanged native limits on Linux and continue complete
remaining-space paths/global replanning, qualified floor/contact, seam/edges
and later actual support. Full-head CCD, motion/order/flow, final-byte replay/
export and complete job/plate/provenance binding remain required.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-constant-section-roof.md`.
Diff: `git show <resolved-implementation-revision>`.
