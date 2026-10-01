# B07 nonnegative nominal stadium depth

Invariant: a rounded section's nominal depth below its axis top cannot be
negative. ADR-0047 tightens the outward interval using that exact geometric
domain, without changing geometry assumptions, budgets or export policy.

Test-first evidence has two distinct red steps. The first test setup was refused
by the transition gap bound; its declared test domain was corrected before
diagnosing the implementation. The valid variable-gap/current-prefix fixture
then fails with MATERIAL_UNION_CELL_LIMIT (exit 42). After the minimal depth
intersection, two cases / 26 assertions pass. Current progress 0.3 and completion
both finish the below/above integrals in exactly two cells and enclose the actual
binary64 deposition amount. A raised rounded bead retains above-target volume
[0.0131619770121793,0.01318686442015476] mm3, strictly between the independent
flat-core/full-width bounds 0.012 and 0.016 mm3.

The application and both test targets build with Apple Clang 21, macOS 26.5.1
ARM64, Release. Combined material suites execute 57 cases / 12384 assertions;
actual-body suites execute 14 cases / 107917 assertions with NoAssertions.
Selected CTest executes 302/302 without skips. Six fresh OFF/ZAA G-code/modal
comparisons pass against pinned stock with the existing normalization and
additive nptop_mode=off allowance. Source inventory/package checks pass.
The existing rounded native fill case now uses 40960 cells / 865083 work, with
above-target upper bound 1.17968e-14 mm3. This is a small reduction and is not
proof that Linux meets its unchanged 20-second fill deadline.

The fresh Linux observation records run 36831523726 at 7f71cc98 completed/failure:
the same native fill deadline failed at 38309 cells / 814580 work, with above
volume [0,0]. Its public check annotation and complete job/run metadata are
archived. The earlier 84a709ef failure is retained in the remainder-hatch archive.
Linux verification of this correction is pending. Windows, independent safety
review and physical execution are NOT_RUN. Full B01-B15 remains active.

Raw outputs are under `build/nonplanar-evidence/B07-stadium-depth-bounds` and
fresh baselines under `build/nonplanar-evidence/B07-stadium-depth-bounds-baselines`.
The evidence archive contains exact argv/completed statuses/exit codes, actual
XML and source/binary/raw/archive SHA-256. Compressed outputs preserve exact
bytes after decompression. Original specification bundle, fixtures, goldens,
printer settings and configured author remain unchanged.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[StadiumDepthBounds]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-stadium-depth-bounds/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-stadium-depth-bounds-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-stadium-depth-bounds-baselines --allow-nptop-off-default
```

Next optimize certified native roof queries without relaxing limits, verify
Linux, then continue complete remaining-space paths, seam/edges and actual
later support. Head CCD, contact/order/flow, final-byte replay/export and complete
job/plate/provenance binding remain required; public export stays blocked.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-stadium-depth-bounds.md`.
Diff: `git show <resolved-implementation-revision>`.
