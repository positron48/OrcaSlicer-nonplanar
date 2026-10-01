# B07 common lower-material joining volumes

Invariant: prove a strictly positive common 3D box inside both selected `D_lower`
beads of one immutable material prefix. Finite eroded ends, original inner losses,
numerical uncertainty and actual current fractions are retained. Future records,
self pairs, non-deposition records, zero-thickness domains and plane-only touching
cannot supply a certificate. ADR-0054 defines additive material-join contract 1.

Three new analytical cases / 1532 assertions pass in the final material suite.
Rectangle/rounded sections, axis and rotated corners, flat/sloped centres and
partial current depositions pass. An independent 113-bit whole-box oracle bounds
the complete projected parameter/normal domain, affine centre and local section
core/radius; derivative losses retain the full actual-prefix domain. It never
calls the production membership predicate or substitutes point samples.
Both cap axes and flat/sloped sources retain every contour corner/closing seam
and both original interior ends. All joins share limits and the original ledger;
no partial list is returned. Generic and whole-cap calls refuse insufficient
volume, exhausted cells/work/depth, stale/cancelled and late publication callbacks,
nonstandard rounding and deadlines. Captured wrappers/limits remain immutable
when caller callbacks change their originals. Positive nominal fill with empty
lower sections still refuses.

The exact unchanged native wedge, actual body, original 0.6 mm and wider 0.75 mm
ROIs, 0.4 mm cap and five-path/161-packet candidate are retained. The extended
native case passes 586 assertions. A high-corner common lower box measures
approximately [3.44447e-5,3.44447e-5] mm3 with 10 cells / 22 work. An independent
whole-box oracle confirms it. Full candidate joining refuses paths 2/3 after
64 cells / 623 work: an independent exact finite-length calculation confirms
all nine relevant low-corner packets are no longer than twice their original
inner XY/numerical loss. Their individual lower sets are empty. No nominal
overlap, wider ROI substitution, zero-loss model or relaxed budget is used to
manufacture native whole-cap approval. All previous S/U/R/C/M/spill remain tested.

The search uses original finite-butt bounds, a shared breadth-first queue over
packet pairs and extra central-box proposals verified continuously. The initial
depth walk and per-pair search exhausted budgets on boundary slivers; subsequent
whole-box search ordering fixes these without raising limits. The independent
oracle's initial full-prefix core bound was too coarse for a local varying-height
box; its corrected local core still charges full-prefix derivative losses.
The low-depth negative now uses a floor skin: a central box can legitimately
certify the old broad domain at depth zero. No negative requirement was removed.

Red missing-API build and search/oracle/native failures are retained. Build11
failed because the test reused an existing local name `prefix`. Native5 then ran
the previous binary, failed with 559 assertions and is not verification of the
new native checks. Native6 follows successful build12. Build14 is the final
application and both test-target build; final material/body/CTest repeat the
current source including late publication cancellation tests and the independent
whole-prefix nonempty-radius check. No failed run is
counted as PASS.

Apple Clang 21, macOS 26.5.1 ARM64 Release application and native targets build.
Final numerical suites pass 80 cases / 16933 assertions. Actual-body suites pass
14 cases / 108474 assertions; the extended native case takes 0.559 seconds there
(standalone 0.599 seconds). These are local observations, not portable guarantees.
Selected CTest executes 325/325 without skips. Six fresh OFF/ZAA G-code/modal
comparisons pass with the original normalization and additive nptop_mode=off
allowance. Source inventory and original package validation pass.

Original cap amount error 1e-4 mm3, gap error 1e-4 mm, width error 0.002 mm,
fill precision 0.001 mm3, original band, material losses and body model remain.
Recorded join searches use at most 4095 shared cells / 200000 work / depth 32 / one-second
deadline and minimum box volume 1e-6 mm3. Search success proves a local declared
inner volume, not a calibrated overlap allowance or physical bonding threshold.
Original 20-second native-fill timeout remains unchanged.

Saved 15:32 UTC Linux observation: contour run 36871942001 at 9a243279 succeeds;
joint-cap run 36878746418 at a165f0c3 is in progress. This new join revision awaits
its own Linux verification. Windows, independent safety review and physical
qualification remain NOT_RUN. No public setting/default, native body/model,
golden, profile, CMake, dependency or normative bundle changes.

Next: reconstruct qualified continuous-run lower material across artificial
packet cuts, preserving variable actual flux/sections and real finite ends.
Then qualify allowable joining excess, floor/body bonding and volumetric seam,
construct the remaining 3D fill and later curved/stepped support. Internal packet
continuity, full-head access, motion/order/flow, worker/job/software binding and
independent final-byte replay remain open. Public export stays BLOCK and the
entire B01-B15 objective remains active; these local joins do not close B07.

Exact command argv/status/exits, XML and source/binary/raw/archive hashes are
in `evidence/B07-material-joins/`. Raw results are in
`build/nonplanar-evidence/B07-material-joins`, with fresh baseline captures in
`build/nonplanar-evidence/B07-material-joins-baselines`. Gzip decompression
preserves original bytes. Source checks are not independent safety review.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialJoin],[FirstCapJoin]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeFirstCapJoin]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B07-material-joins/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B07-material-joins-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B07-material-joins-baselines --allow-nptop-off-default
```

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-material-joins.md`.
Diff: `git show <resolved-implementation-revision>`.
