# B14: source-only native view with child Print preparation

Date: 2026-10-03. Parent: `9260ff565427cb0e659fbb4cba081b1f8e8a0366`.
Delivery: `git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B14-source-worker.md`.
ADR: ADR-0095; critical author review: B14-source-worker-review.md.
Independent safety review remains pending. Full B01-B15 remains active.

## Implemented invariant

GUI analysis no longer builds a second Model/config copy or applies an analysis
Print in its background host process before launching the child. It captures the
bounded immutable original source, request and exact editor/plate/mode identity,
then starts a private source-only display attempt. No host GuardedJobTask is
manufactured. The child reconstructs exact published source, checks original
source policy, applies actual Print and checks resolved global/region policies
under the existing child watchdog and OS CPU/file limits.

Internal worker protocol 2 adds host_view=[revision, attempt, exact_view_identity]
while legacy Print-bound protocol 1 remains. Both paths share source codecs and
response checks. Only the existing fixed thirteen non-slicing auth/network/log/
time keys are omitted in transport; original private source remains in view
identity. Exact editor resource and observed source files are required. The red
editing-resource test observed an accepted missing resource before the fix;
corrected capture refuses missing/changed bytes and foreign resource kinds.

Display owner begin/finish/invalidate stays serialized by the GUI. Its atomic
validity reaches the watchdog, edits/cancel/destruction invalidate and every
adoption consumes the token once. A late/foreign callback cannot revoke a newer
owner. Current live identity and epoch must match. Capture progress precedes
child Print.apply; policy refusals expose keys without hook bodies or partial
replay. Existing queue, modal event processing and weak callback remain.

No mathematical IR, public request/report, printer/profile or 3MF format changes.
No new export route, physical qualification or Verified transition. The new
request fixture is extracted from the original native-worker editing request;
no original mesh/golden/margin/negative expectation is changed.

## Qualified evidence

Frozen exact source/closure/binary and command records are in
`evidence/B14-source-worker/source-manifest.json` and `commands.json`;
logs/large files use lossless gzip. Platform: macOS ARM64, AppleClang
21.0.0.21000101, CMake 4.3.1, Ninja Multi-Config, Release.

- Final C++ build2: exit 0, 50.688 s. Build3 confirmation: exit 0, 13.498 s;
  unchanged build: exit 0, 13.677 s, no C++ compile/link. Initial test-helper
  signature error and causal missing-editing-resource red are retained.
- Source-only focus: 4 cases /118 assertions, exit 0, 3.112 s, real child and
  separate native reference with no analysis Print in the source host fixture.
- Final expanded focus, seed 1287612491 with NoAssertions: 17 cases /9476
  assertions, exit 0, 31.801 s, including legacy worker/view and watchdog probes.
- Final selected CTest discovery/execution: 475/475, zero failure/disabled/skip,
  exit 0, 145.607 s. Other upstream suites are not newly qualified.
- Source tests retain original 2098 final movements, source auth omission,
  exact editing resource, single adoption, replacement/foreign owner/invalid
  begin, one-bit settings/raw whitespace/signed zero/mode/epoch/cancel,
  malformed mesh/matrices/annotations/request/software/version, actual
  Capture cancellation/invalidation/replacement, original hook/prime-tower
  policy and unconfirmed-units refusals without partial output.
- All six old actual-PID watchdog cases remain: five failures stop inside a
  blocked callback, a healthy child stays alive until explicit cancellation,
  and every PID is absent after return. No probe is packaged.
- Sixteen independent inventory/job/lineage/final-byte/worker/view/source oracle
  commands exit 0. New source oracle independently frames actual published
  source mesh/config/annotations and parses separate native final bytes;
  all seven source/request/editor/owner/movement mutations refuse. Its owner
  comparison binds separately recorded metadata, not an independent proof of
  omitted private settings or permission to export.
- Ten fresh actual main Orca CLI cases including refusal/SIGINT pass.
- Six fresh strict original stock OFF/ZAA comparisons pass; the existing
  OFF-default allowance is the only allowance.
- Fifty of 51 parent candidate files remain byte exact; only software-bound
  native-job-report.json changes. Original candidate/margins/refusals remain.
- Default/source candidate: 2098 movements, SHA256
  `c330363d909d5907a52b151a7ca499d1ce72681cc0185edd6f9c312bef12686b`.
- Compiled inventory: 18,300 source/resource/build files, 2,435,409 bytes,
  SHA256 `8fe0dd5e852802becdb4afa386b63541fe201d2cd65f05b3455d15147af7197d`.
- Fresh isolated bundle `b14-source-worker-delivery` passes actual network/
  write/datadir probes and contains the exact worker. Main binary SHA256
  `148ed2ecfbfff57a0f66a1a92482b772b047e49edda74feb358ca743a63daa3f`;
  worker `c612bf7ba626c32bceff914f605ee544cc72b98e677fccaa4e085dfb43254efc`.
- Native GUI runtime: **NOT_RUN for this source**. Native UI twice reported a
  locked Mac, unlock was requested once, software verification continued.
  Exact reason is preserved in gui-not-run.json. Previous B14-native-gui
  synthetic observations are historical and do not qualify this new path.
- Original pinned-source and normative package audits pass. The fixed report
  remains **4 PASS/RUN +13 UNKNOWN/NOT_RUN**, overall UNKNOWN, export BLOCK.

Reproduction with fresh evidence directories:

```sh
cmake --build build/arm64 --config Release --target nonplanar_analysis_worker nonplanar_tests fff_print_tests nonplanar_rate_audit OrcaSlicer -j6
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeAnalysis],[NativeAnalysisJson],[NativeAnalysisWorker],[NativeAnalysisView],[NativeAnalysisSource]' --rng-seed 1287612491 --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir <fresh-evidence> --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/native_source_oracle.py --fixtures <fresh-job-fixtures>
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 <fresh-baselines> --allow-nptop-off-default
```

## Remaining work

Run the new isolated GUI after unlock, including source/plate refusals, exact
first/last movement/time, edit/cancel/close and idle queue. Full GUI/P2 remains
open. Parent source/config capture, source-file observation and JSON capture/
construction/parsing are still cooperative and lack hard RSS containment. The
child's actual Print.apply is now supervised, but sampled RSS does not establish
instantaneous hard macOS memory containment. OS termination/observation calls,
descendants and parent-crash cleanup are unqualified. Compiled controls include
cached GIT_COMMIT_HASH=a77fb582; complete software identity remains UNKNOWN.

Linux new-head execution, Windows runtime, independent review and physical tests
remain pending. Continue complete source/import/3MF provenance, full domain
editing/visualization, cap/later passes, qualified contact/seams, whole-job/order/
geometry, all seventeen domains and atomic publication. Standard U1 head and
0.4-mm nozzle are known; software work proceeds independently.
