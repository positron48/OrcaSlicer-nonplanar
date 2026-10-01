# B07 first-candidate width/pitch and excess replanning

Invariant: replace the complete prospective first candidate with a narrower
nominal width and redistributed transverse centres within the unchanged outer
band, retaining actual body/target, finite longitudinal extents, line count and
later paths. Reconstruct every whole-footprint roof/gap/amount proof and joint
S/U/R/C/M/spill; require positive commanded-volume and multiplicity-excess
reductions, a bounded declared C loss/M increase, and the outside-target limit.
ADR-0051 records the owned-line-width contract and consumer migration.

Three analytical cases / 279 assertions pass. Both axes retain source/target
identity, separate old/new ledgers, original generation policy and later widths.
Independent 113-bit actual stadium arithmetic encloses S/U/R. Four redistributed
lines repartition their displaced interior owners; independent circular-lens
arithmetic encloses their measures, including a real small C loss explicitly
accepted within the 0.001 mm3 candidate-quality tolerance. A wider loss is refused
with FIRST_HATCH_WIDTH_REPLAN_COVERAGE_LOSS. Missing material remains positive;
no quota becomes commanded V/E. Finite-cell, remaining-space and end-replan
consumers use the new owned widths while retaining the original generation policy.
Mutable wrappers, invalid/equal/wider/too-narrow widths, insufficient reduction,
shared limits, deadline, stale revision, cancellation and rounding reject as
appropriate. Test-first missing-API failure and the later test-only multiprecision
abs qualification build error are retained; no proof budget was relaxed.

The unchanged native wedge and actual sliced flat bead core pass the extended
213-assertion case. Width 0.45 -> 0.40 mm redistributes the two first lines while
preserving their finite ends and original 0.05 mm band. Commanded S and repeated
R each decrease by approximately 0.0398967 mm3. Nominal C remains approximately
0.181000 mm3; its measured change is about +2.46731e-8 mm3, arising from actual
packet amounts rather than an assumed exact ideal-area identity. M remains
approximately 0.0709876 mm3. The replacement uses 70 shared proof cells / 17985
work; the whole native case takes 0.310 seconds locally, without a portable
timing guarantee. Requested global amount error 1e-4 mm3, gap error 1e-4 mm, width error
0.002 mm, fill precision 0.001 mm3, 65535 cells / 2M work and five-second root
limit remain unchanged. Material path width does not redefine the 0.4 mm nozzle.

Affine-hatch contract 3 -> 4 makes owned line widths authoritative after
replanning, retaining policy.width as original generation provenance. First-
bead 3 -> 4, finite-cell 1 -> 2, remaining-hatch 1 -> 2 and first-layer 1 -> 2
consumers are rebuilt; add first-width-replan version 1. No persisted cache,
IR, profile or 3MF representation exists for these internal snapshots. Native
original generation, later prospective geometry and OFF/ZAA behavior retain their
contracts. Actual later support must still be reconstructed independently.

Application and both test targets build with Apple Clang 21, macOS 26.5.1
ARM64, Release. Material suites pass 70 cases / 13204 assertions; actual-body
suites pass 14 cases / 108101 assertions. Final selected CTest passes 315/315
without skips; six fresh OFF/ZAA G-code/modal comparisons pass against pinned
stock with existing normalization and additive nptop_mode=off allowance. Source inventory and
original package validation pass. Original normative bundle, models, goldens,
profiles, CMake and dependencies are unchanged.

Linux run 36853913690 at 361ccdc8 now succeeds: native selected suites, STL CLI
and application pass. Run 36847680702 at 2d8bebaa succeeds with the original
20-second fill timeout. Run 36858083109 at 05f4a887 is still building native tests
in the saved 12:23 UTC observation. This new width replan awaits its own Linux
verification. Windows, independent safety review and physical qualification
remain NOT_RUN; public guarded export remains BLOCK and full B01-B15 active.

The old hypothetical cap is not laid material. This does not repair an actually
laid partial prefix, solve the still-positive 3D deficit, construct perimeter/
seam, qualify finite side/floor contact or head/nozzle access, select a legal
order/flow, establish curved/later actual support, or permit export. Job/plate/
software binding and independent final-byte replay remain required. Reducing
R does not prove allowable contact or resolve M.

Exact argv/status/exits, XML, raw/source/binary/archive hashes are retained in
`evidence/B07-first-hatch-width-replan/`; raw files remain in
`build/nonplanar-evidence/B07-first-hatch-width-replan`, with fresh captures in
`build/nonplanar-evidence/B07-first-hatch-width-replan-baselines`. Gzip files
retain original bytes after decompression. Author checks are not an independent
safety review.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[FirstHatchWidthReplan]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeFirstHatchWidthReplan]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-first-hatch-width-replan/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-first-hatch-width-replan-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-first-hatch-width-replan-baselines --allow-nptop-off-default
```

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-first-hatch-width-replan.md`.
Diff: `git show <resolved-implementation-revision>`.
