# B07 complete first-candidate finite-end replanning

Invariant: replace the complete prospective first candidate with finite
longitudinal butts inside the same supported ROI and unchanged boundary band.
Recompute every complete footprint roof/gap/amount and whole union/fill, then
require positive measured C gain and M reduction. Original actual body and
first target are retained; old hypothetical cap material is not appended.
ADR-0050 documents extent provenance and the internal contract migration.

Three analytical cases / 183 assertions pass. Both first axes retain transverse
centres, width, boundary band, source body and target, but own a fresh replacement
hatch/ledger. Independent 113-bit actual stadium measures enclose S/U/R. The
initial exact area-scaling test was invalid because packet volume rounding may
change by one ulp; its failed output is retained and the independent oracle now
uses actual new amounts. Invalid limits are rejected before reduction. Mutation,
insufficient gain, already-replanned extent, shared path/record/packet/work/cell
limits, timeout, stale revision, cancellation and rounding refuse snapshots.
A current narrow ridge only inside the new longitudinal domain rejects the
complete replan while the old candidate still succeeds; its future remainder
and current event before the new start remain absent.

The unchanged native wedge and actual sliced flat bead core pass the extended
64-assertion case. C increases from approximately 0.138283 to 0.181000 mm3;
M decreases from 0.113705 to 0.0709877 mm3. The guaranteed measured gain/reduction
is approximately 0.0427173 mm3. The replacement consumes 79 shared cells / 18318
work; the whole case takes 0.304 seconds locally. Actual commanded S/U/R are
independently enclosed. Requested 1e-4 mm3 global amount error, 1e-4 mm gap
error, 0.002 mm width error, 0.001 mm3 fill precision, 65535 cells / 2M work
and the five-second root deadline remain unchanged. The old candidate's immutable
source, target and ledger remain distinct from the replacement.

`affine_hatch_contract_version` changes 2 -> 3 with explicit first-pass extent
CapsuleInset/FiniteButtInset. Original hatch generation remains CapsuleInset;
all current consumers are rebuilt. No persisted cache/IR/3MF/profile representation
exists for these internal snapshots. Whole source/strip volume quotas are
unchanged and are never used as commanded extrusion amounts. Later prospective
hatches are retained; actual later support must still be reconstructed.

Application and both native test targets build with Apple Clang 21, macOS
26.5.1 ARM64, Release. Material suites pass 67 cases / 12925 assertions; actual-
body suites pass 14 cases / 107952 assertions. Selected CTest passes 312/312
without skips; six fresh OFF/ZAA G-code/modal comparisons pass against pinned
stock with existing normalization and additive nptop_mode=off allowance. Source inventory and original package validation pass. Original
specification, fixtures, goldens, profiles and build/dependency settings are unchanged.

Linux run 36847680702 at 2d8bebaa completed successfully: selected native tests,
native STL CLI and application build all pass with the original fill timeout.
This verifies the earlier constant-section optimization; prior deadline failures
remain historical evidence. Run 36853913690 at 361ccdc8 is still building native
tests in the saved 11:37 UTC observation. This new end replan is not yet Linux-
verified. Windows, independent safety review and physical qualification remain
NOT_RUN. Public guarded export stays blocked and full B01-B15 remains active.

Finite material extent is not a head/nozzle working-zone certificate. The
replacement is prospective; an actually laid partial cap requires a separate
remaining-space construction. Residual 3D voids, overlap qualification,
perimeter/seam, curved/later actual support, head CCD/motion/order/flow, job/plate/
software provenance and final-byte replay/export remain required. The measured
native deficit is reduced, not resolved.

Exact argv/status/exit, XML, raw/source/binary/archive hashes are under
`evidence/B07-first-hatch-end-replan/`; raw files remain in
`build/nonplanar-evidence/B07-first-hatch-end-replan` and fresh captures in
`build/nonplanar-evidence/B07-first-hatch-end-replan-baselines`. Compressed logs
and G-code retain original bytes after decompression.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[FirstHatchEndReplan]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeFirstHatchEndReplan]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-first-hatch-end-replan/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-first-hatch-end-replan-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-first-hatch-end-replan-baselines --allow-nptop-off-default
```

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-first-hatch-end-replan.md`.
Diff: `git show <resolved-implementation-revision>`.
