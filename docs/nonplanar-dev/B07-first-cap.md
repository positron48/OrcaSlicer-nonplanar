# B07 joint first contour and interior hatches

Invariant: replace original outer hatch owners with the closed contour, retain
original interior owners, trim their finite butts to contour centre extents and
re-prove every whole footprint against the original actual body. Measure all
paths in one fresh ordered cap ledger with joint S/U/R/C/M/spill. Retain original
target, boundary band and later prospective geometry. ADR-0053 records the
protected first-cap API and certified multiple-row union domain.

Three new analytical cases / 1515 assertions pass. Both hatch axes and flat/
sloped surfaces retain immutable source, indices and original finite extents.
Four contour edges have no false infill owner; replaced boundary indices are
explicit. An independent 113-bit merged-core identity encloses total S/U/R/C,
including multiple occupancy. Flat candidates improve measured C/M relative to
a contour on the same target. Independent multiple-row circle/rectangle amounts
reject unequal interior profiles, unmerged cores and partial current events.
The ledger matches declared path order and actual amounts, including nonzero
unqualified travel connectors. No synthetic joint proof is assembled from sums.

No interior, width mismatch, clipped/mismatched box, mutable wrappers, shared
path/record/packet/roof/cell/work limits, deadline, revision, cancellation and
rounding refuse as appropriate. A local ridge outside all contour footprints
leaves the contour positive but rejects the later infill; future material is
absent. The initial ridge target integral at 0.001 mm3 was too coarse for the
unchanged 0.001 mm3 fill budget. Its requested source integral error was tightened
to 0.0001 mm3 without changing geometry or relaxing any fill/solver limits;
the intended later-infill refusal then passes. Original failed outputs remain.

The same unchanged native wedge, body paths and 1.0 mm native body-line fixture
remain tested in the extended 559-assertion case. A new 0.75 mm wide ROI is
independently enclosed by an actual sliced bead's nominal flat core. Original
0.6 mm ROI and all earlier obligations remain. The new three-owner source grid
at 0.4 mm cap width replaces owners 0/2 with the contour and retains owner 1.
Its joint candidate has five paths / 161 packets, S approximately 0.370959 mm3,
C [0.207411,0.207818] mm3, M [0.107167,0.107574] mm3 and real positive outside
volume [0.000161830,0.000359115] mm3. Independent actual-source section integrals
enclose every path amount. This wider target differs from prior narrow candidates;
no native coverage-gain comparison across those ROIs is claimed.

The joint native measurement uses 293 shared cells / 38258 work. The standalone
whole case took 0.605 seconds; final Catch case timing is 0.567 seconds. These
local observations are not portable timing guarantees. Original
global amount error 1e-4 mm3, gap error 1e-4 mm, width error 0.002 mm, fill precision
0.001 mm3, 65535 cells / 2M work and five-second root limit remain unchanged.
The previous narrow contour's added group validation now accounts 30690 work
(historical contour report: 30635); its material measurements remain consistent.
The native body-line width is a software fixture, not a U1 configuration write.

Test-first missing-API and test-only typed-value/native-member/duplicate-local
build failures are retained. The native1 attempt used the previous binary after
a failed build, found no matching new tag and exited 2; it is not verification.
Native2 follows successful build7 and executes the extended case. No red/empty
run is counted as PASS. Add first-cap contract 1; existing owner/union/fill result
contracts retain their meaning. No persisted format, public setting/default,
normative bundle, model, golden, profile, CMake or dependency changes.

Final application and both test targets build with Apple Clang 21, macOS 26.5.1
ARM64, Release. Final material suites pass 77 cases / 15401 assertions.
Actual-body suites pass 14 cases / 108447 assertions. Selected CTest passes
322/322 without skips; six fresh OFF/ZAA G-code/modal comparisons pass against
pinned stock with existing normalization and additive nptop_mode=off allowance.
Source inventory and original package validation pass.

Linux run 36861891679 at 4e6cddd7 succeeds in selected native suites, STL CLI and
application. The saved 14:36 UTC snapshot shows run 36871942001 at 9a243279
building native tests. This joint cap awaits its own Linux verification. The
original 20-second native-fill timeout is unchanged. Windows, independent safety
review and physical qualification remain NOT_RUN.

M and R remain positive. Joining material/contact, complete 3D repair, stepped/
curved and later actual support, full-head access, legal motion/order/flow,
job/plate/software binding and independent final-byte replay remain required.
This is a prospective first candidate, not actual partial-cap replacement or a
filled pass. Full B01-B15 remains active and guarded export stays BLOCK. Standard
head and nominal 0.4 mm U1 nozzle remain user declarations, without qualification.

Exact argv/status/exits, XML and source/binary/raw/archive hashes are retained
in `evidence/B07-first-cap/`; raw files are in
`build/nonplanar-evidence/B07-first-cap`, with fresh captures in
`build/nonplanar-evidence/B07-first-cap-baselines`. Gzip files preserve original
bytes after decompression. Author checks are not independent safety review.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[FirstCap],[InfillLoopUnion]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeFirstCap]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-first-cap/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-first-cap-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-first-cap-baselines --allow-nptop-off-default
```

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-first-cap.md`.
Diff: `git show <resolved-implementation-revision>`.
