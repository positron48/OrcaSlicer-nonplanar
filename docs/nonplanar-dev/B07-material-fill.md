# B07 nominal target-fill reconciliation

Invariant: bound missing target volume separately from material outside the
actual roof-to-affine-surface solid; equal total volume cannot approve fill.
ADR-0043 adds protected internal material-fill contract 1, reusing the finite
union and actual nominal roof integrators. Four implementation/test files,
this ADR/report and execution records are the permitted change scope. No
serialized IR/project/profile, fixture, dependency or export route changes.

Five new analytical cases / 126 assertions pass. Independent rectangular,
triangle and circular integrals test exact fill, current fractions 0/0.3/0.5/1,
below-roof spill, transverse affine slope and a rounded actual roof. A displaced
candidate has the same total target amount but positive missing and outside
volumes simultaneously. Private proof ownership survives caller wrapper and
callback mutation. Missing proofs, different revision/origin, a partial body
claiming a full parent, incompatible XY/incomplete Z domains, insufficient input
precision, work/cell limits, stale revision, cancellation, timeout and changed
rounding publish no snapshot. Provisional measures never approve a result.

The existing actual native case is extended to 54 assertions. It captures,
partitions and natively slices the unchanged exact 1:16 wedge, reconstructs
all laid body beads, and captures all 14 first-line / 2109 candidate packets.
Ordinary occupied union still meets its 0.01 mm3 / ten-second policy. Its
bounds are now [1.31221597619643537,1.32221578176234544] mm3, with unchanged
32213 cells / 1598991 work units; correlated floor coefficients tighten a few
ULPs. The commanded sum remains 1.39569413683378021 mm3, independently summed
in 113 bits. Declared connector travels do not approve an executable order.

For the target, the same actual source is integrated more precisely at
0.001 mm3, 65535 cells and five seconds. The integral evaluates 10377 cells
and gives approximately [2.57885,2.57985] mm3. Fill reconciliation requests
0.02 mm3, 65535 cells, 2000000 work units and one shared twenty-second deadline
for both clipped components and preparation. It uses 41056 cells / 885187
work units. Approximate retained intervals in mm3 are:

| Measure | Lower | Upper |
|---|---:|---:|
| Covered target | 1.29275 | 1.30712 |
| Missing target | 1.27173 | 1.28710 |
| Outside target, within the owned XYZ box | 0.0150945 | 0.0194677 |
| Below actual nominal body roof | 0.0150945 | 0.0173445 |
| Above target affine surface | 0 | 0.00212321 |

The individual native process takes 25.317 seconds including fixture, candidate,
union and target preparation. The complete body suite takes 37.534 seconds;
these are whole-process timings, not the fill-only deadline. All published
interval widths are checked separately against 0.02 mm3. These candidates
have substantial proven underfill and positive outside volume; this is useful
diagnostic evidence, without a complete accepted fill planner.

The missing API produces the expected test-first link failure. Native failures
expose declared-parent mismatch and work exhaustion. Retained profiling shows
fine diagonal roof cells spend nearly all work in overlapping AABB traversal.
The final implementation instead refines the actual protected body source with
the shared continuous roof helper, retains candidate sets through subdivision,
and alternates body/candidate directions. Correlated floor coefficients remove
false longitudinal uncertainty. Midpoint probes choose a direction only; every
bound remains continuous. No native precision, deadline or hard budget was
increased to obtain acceptance; the initial 0.04 mm3 fill precision was tightened
to 0.02 mm3. One broad text replacement caused three compilation errors and was
corrected before tests. A newly written pure-circle example was outside the
existing admitted rounded-section domain; the independent oracle now exercises
a supported rounded shoulder. Failed logs remain archived. Temporary profiling
output and the tree experiment are absent from final source.

Apple Clang 21 / macOS 26.5.1 ARM64 / Release builds OrcaSlicer and both test targets.
Material suites execute 45 cases / 11988 assertions and actual-body suites
13 cases / 107878 assertions, both with NoAssertions. Selected CTest executes
289/289 without skips. Six fresh OFF/ZAA G-code/modal comparisons match pinned
stock with the established normalization and additive nptop_mode=off. Source
inventory and immutable package validation pass. Exact argv, exit/status,
raw failure/success logs, XML, source and binary hashes are under
`evidence/B07-material-fill/`; raw output remains under
`build/nonplanar-evidence/B07-material-fill/` and the separate baseline directory.
Large text/G-code files are losslessly compressed in the committed archive.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialFill],[MaterialUnion]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeMaterialFill]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-material-fill/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-material-fill-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-material-fill-baselines --allow-nptop-off-default
```

Author review checks immutable source association, actual-prefix roof definition,
finite fractional butts, source-candidate pruning, disjoint clipped windows,
unit-Jacobian shear, outward interval conservation, separate nonnegative deficit
and spill, precision allocation and shared cancellation/deadline/work accounting.
Independent safety review remains pending. The native whole fill/union does not
yet have an independent whole-native-geometry oracle; generic analytical oracles
and the native amount oracle do not replace it. Full B13 software provenance
is pending: the retained configure label is 31461d50, with this exact build
identified by source and binary hashes. The saved Linux snapshot records
successful e2030e15 runs 36812147275 and 36819847014; 4b3da47d run 36820170446
is in progress. None verifies this fill revision. Windows, physical qualification
and full-cycle performance are NOT_RUN.

Next: localize missing/excess regions and construct finite paths that resolve
them, qualify finite-width floor/contact, then perimeter/seam and actual
subsequent support. Wider/curved paths, legal order/travel, full-head CCD,
segment/flow limits, whole-job/plate binding and final-byte replay/export remain
open. The public guarded-hybrid blocker is retained; the entire B01-B15 objective
stays active.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-material-fill.md`.
Diff: `git show <resolved-implementation-revision>`.
