# B06 first-cap floor/body interface

Invariant: every complete varying flat floor uses the original actual body
prefix and a positive-volume D_lower anchor under the complete original ROI.
Bound continuous nominal floor/roof separation and support-plane distance with
original numerical uncertainty in XYZ. The prospective cap and future body
cannot supply their own support. ADR-0056 defines protected additive contract 1.

Three new analytical cases / 4669 assertions pass. Both cap axes and flat/sloped
surfaces retain actual binary amount, XY length, affine Z/gap and rounded flat
width. Independent 113-bit bounds verify anchor volume/whole lower box, complete
maximum-width floor enclosures, minimum true width and affine separation ranges.
These are continuous bounds, not sampled vertices or production-predicate replay.
The former join oracle is reused through its extracted whole-box helper; its
original derivative and finite-butt semantics remain unchanged.

Actual-prefix tests include a future higher rectangle and a partial current
rectangle whose laid half remains outside the ROI. Neither changes the current
floor; completing it changes the expected floor from 1 to 1.01 mm. Absent 3D
anchors, too-large support distance, too-narrow floors, absent input, invalid
policy/depth, record/patch/work/cell limits, stale/cancelled and late publication,
timeout and nonstandard rounding provide no complete snapshot. Caller callbacks
can mutate their wrappers/policy/limits without altering the captured result.

The original coarse analytical slope correctly refuses: numerical error
0.00348669 mm exceeds the explicit 0.003 mm interface allowance. Its positive
case tightens packet width approximation from 0.005 to 0.002 mm, preserving all
other budgets and source geometry. An initial negative incorrectly expected a
flat zero-error floor to fail a 1e-10 gap allowance; that geometry legitimately
passes. The retained negative uses actual excessive uncertainty instead.
Initial missing-API red build and both failed test executions remain evidence.

The unchanged native wedge has five paths/161 packets and all 161 interfaces
pass. Its body anchor measures approximately [0.03,0.03] mm3, with 162 shared
cells / 5661 work. The extended native case passes 2912 assertions, standalone
0.622 seconds locally. Original 0.4 mm cap, 0.6/0.75 mm ROIs, body, band,
amount/gap/width/fill tolerances and actual packet geometry remain unchanged.
The six run joins and 53-packet lower coverage still pass; the original per-event
lower-corner refusal still retains nine empty packets and 64 cells / 623 work.
The original real deficit/spill and S/U/R/C/M assertions remain.

Root limits remain 4095 cells / 200000 work / depth 32 / one-second deadline.
The explicit simulation policy is 0.02 mm anchor depth, 0.125 mm maximum support
distance, 0.003 mm nominal gap/overlap and 0.05 mm minimum flat width. These
are geometric test assumptions, not measured U1 contact parameters. Source
outer-AABB pruning reduced native work from 129792 to 5661; every surviving
whole roof/coverage query and original loss remains. No future endpoint is used.

Build6 is the final Apple Clang 21 macOS 26.5.1 ARM64 Release application and
native-target build. Final material suite: 86 cases / 24261
assertions. Actual-body suite: 14 cases / 110800 assertions. Selected
CTest: 331/331 without skips. Six fresh final-binary OFF/ZAA
G-code/modal comparisons retain original normalization and only the established
additive OFF-default allowance. Source inventory and original package validation
pass. Final confirmations follow the complete maximum-width floor correction;
earlier minimum-strip-only suites are retained as preliminary evidence.

Saved 17:07 UTC Linux observation: material-join run 36887521770 at d02d7544 is
in progress; continuous-run run 36894070655 at 3d3f97a4 is pending. This interface
stage awaits its own Linux verification. Independent safety review, Windows and
physical qualification remain NOT_RUN. There are no profile/public default,
CMake/dependency/golden or normative bundle changes.

Next: qualify complete rounded-shoulder/seam volumes and allowable repeated
material, construct remaining 3D fill and reconstruct later actual support.
Flat-floor support alone does not prove physical bonding, a sealed full layer,
legal head access/contact/order/flow or export permission. Full B01-B15 remains
active and public export stays BLOCK.

Exact argv/exits, XML, source/binary/raw/archive SHA-256 and byte-preserving gzip
mappings are in `evidence/B06-cap-interface/`. Raw results are in
`build/nonplanar-evidence/B06-cap-interface`; final baselines are in
`build/nonplanar-evidence/B06-cap-interface-confirmation-baselines`. Preliminary
build5 baselines remain separate. All consumers rebuild; protected snapshot
contract 1 adds no persisted cache, IR, 3MF or profile schema.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[FirstCapInterface]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeCapInterface]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B06-cap-interface/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B06-cap-interface-confirmation-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B06-cap-interface-confirmation-baselines --allow-nptop-off-default
```

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B06-cap-interface.md`.
Diff: `git show <resolved-implementation-revision>`.
