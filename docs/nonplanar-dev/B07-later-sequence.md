# B07: ordered later paths on actual sequential material

Date: 2026-10-03. Parent: `eed679c2121282938b8b8a571cbcb83aa2b30a63`.
Delivery: `git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-later-sequence.md`.
ADR-0096; separate critical author review: B07-later-sequence-review.md.
Independent safety review remains pending. Full B01–B15 remains active.

## Implemented invariant

The new private C++ `plan_next_cap_sequence` result owns requested later paths
and exact before/after material. Each local request reuses the captured original
fixed width and alternating pass direction, proves actual whole-footprint Lower
support/vertical gap, mandatory whole-domain normal spacing and finite-width
nominal roof/dose, then appends that bead before assessing the next request.
Every previous canonical row remains exact. No prospective future material or
old dose is reused as newly printed support. A failure returns no partial program.
Root work/cell/deadline/cancellation/current limits span all support, bead and
append stages, preserving tighter child/packet policies and final publication
polling. Callback replacement of caller inputs/limits cannot change the owned work.

Continuous run-union coverage now skips impossible single-run proposals when the
existing outward enclosure cannot contain the full query. Lower uses the same
original loss/error expansion. Keep the intersecting run for union subdivision;
acceptance still requires the original complete inequalities. The observed old
single-run attempt exhausted 4095 cells on transverse missing material before
union subdivision. No limit/margin/geometry or original negative is weakened.

This is an internal ordered-local-path factory exercised by analytical tests.
The common native controller still constructs its original first-cap candidate.
Complete later-layer/cap generation, request capture, native lineage, replay and
job/export integration remain pending; advancing a pass proves no full-layer fill.
No persisted IR/profile/3MF/cache/worker/report format or public export change.

## Qualified evidence

Exact source/closure/binary hashes, argv/exits, actual test IDs, JUnit and lossless
raw mappings: `evidence/B07-later-sequence/source-manifest.json` and `commands.json`.
Raw execution directory: `build/nonplanar-evidence/B07-later-sequence`.
Platform: macOS ARM64, AppleClang 21.0.0.21000101, CMake 4.3.1, Release.

- Final build-final5: exit 0, 28.444 s. Unchanged build: exit 0,
  13.292 s, no C++ compile/link. All five application/worker/test/audit consumers built.
- Four new cases /3265 assertions with NoAssertions: exit 0, 0.167 s.
  Both axes and flat plus separate .004-mm-rise affine sources plan two actual
  ordered later paths; the sloped second path has nonzero Z displacement.
  Independent 113-bit run/event inequalities certify whole support and terminal
  normal volumes, with complete disjoint partition measure and command-dose sums.
- The unchanged original .04-mm-rise affine sources refuse transverse roof
  approximation at the unchanged .001-mm gap-error limit. A nominal-enclosure
  shoulder box is independently outside its stadium and refuses nominal/Lower
  coverage. Duplicate area, missing/incomplete source, skip/backward/outside/
  degenerate/NaN requests, limits below successful work/cells, record/path limits,
  root/child cancellation/stale predicates, exceptions, deadlines, rounding,
  callback input mutation and final publication cancellation remain refusals.
- Selected CTest: 479/479, zero failures/skips, exit 0, 144.678 s;
  242 nonplanar_tests cases +237 fff_print_tests cases. Other upstream suites are
  not newly qualified. The initial unprepared-directory run failed only at
  evidence writes; corrected fresh precreated directories pass the entire gate.
- Sixteen independent inventory/job/lineage/final-byte/worker/view/source oracle
  commands exit 0 on final-jobs2/final-candidates2. An initial wrapper pointed at
  incomplete failed-gate diagnostics; it is preserved and excluded.
- Ten actual main Orca CLI cases pass, including policy refusals/SIGINT.
- Six fresh strict original stock OFF/ZAA G-code/modal-replay pairs pass; the
  established additive OFF default is the only allowance.
- Fifty of 51 parent candidate files remain byte exact; only the software-bound
  native-job-report.json changes. Existing default 2098-movement candidate SHA256
  remains `c330363d909d5907a52b151a7ca499d1ce72681cc0185edd6f9c312bef12686b`.
  These existing final-byte checks do not qualify the new later program's replay.
- Compiled inventory: 18,300 files /2,435,409 bytes, SHA256 `2e555ac4445e007b9d5a86614cbb9efb6caad867735f9cf83fa5c7ce5c309ac1`.
- Fresh isolated bundle `b07-later-sequence-delivery` passes actual write/network/
  datadir probes. Main binary SHA256 `96c531a75c3e2d45b05cf61907f2d45bf9983882cc4d9b73c3d8c9da440d7d76`; worker `adc9a23c0f9089b5de74219db1d4ad6d32f6a7fc8125c80913f3bd34f0e31277`.
  GUI runtime is NOT_RUN; prior GUI observations remain historical.
- Actual six PID watchdog observations remain valid. Five failures stop inside
  a blocked callback; healthy stays alive until cancellation, all absent on return.
- Pinned source audit and immutable normative package validation pass. Fixed17
  remains 4 PASS/RUN +13 UNKNOWN/NOT_RUN, overall UNKNOWN and guarded export BLOCK.

The missing-API test-first build, union exhaustion, test-only compile/partition
corrections, original steep refusals, stale-binary focus2 and failed setup/wrapper
runs are retained. `excluded-observations.json` identifies their boundaries;
none supplies qualified final success. Existing models/goldens are untouched.

Fresh reproduction (create diagnostic directories before CTest):

```sh
cmake --build build/arm64 --config Release --target nonplanar_analysis_worker nonplanar_tests fff_print_tests nonplanar_rate_audit OrcaSlicer -j6
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[NextCapSequence],[UnionEnclosurePruning]' --warn NoAssertions
mkdir -p <fresh-job-directory> <fresh-candidate-directory>
NPTOP_JOB_EVIDENCE_DIR=<fresh-job-directory> NPTOP_CANDIDATE_EVIDENCE_DIR=<fresh-candidate-directory> python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir <fresh-ctest> --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 <fresh-baselines> --allow-nptop-off-default
```

## Remaining work

Capture/bind the actual later program into the common controller, protected native
lineage and independent final bytes. Construct and certify complete filled later
layers/cap, allowable excess, shoulder/seam/contact and accessible directions,
full head and connectors, full job order/geometry and all mandatory domains.
Retain the original native old-neighbour/contact refusal. Complete source/import/
3MF and domain UI, hard resource/process-tree/crash containment, complete software
identity and atomic publication remain open. Root callbacks are cooperative; failed
bead counters are not exact failed-work measurements. Cached GIT_COMMIT_HASH still
reads a77fb582; complete software identity remains UNKNOWN. New-head Linux,
Windows runtime, GUI/physical and independent review remain pending. Standard U1
head and 0.4-mm nozzle are known; software does not await firmware/material brands.
