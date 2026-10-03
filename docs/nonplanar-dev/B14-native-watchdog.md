# B14: native child supervision independent of progress callbacks

Date: 2026-10-03. Parent: `9137fde52edb8316ca73734097b9723bb0ab030f`.
Delivery: `git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B14-native-watchdog.md`.
ADR: ADR-0094. Critical author review: B14-native-watchdog-review.md;
independent safety review remains pending. Full B01-B15 remains active.

## Implemented invariant

The native worker poll previously stopped checking budgets while it invoked a
host progress callback. The corrected red regression observed the actual child
PID still alive inside a blocked callback after the root deadline. The first
red launch allowance was too short to reach the callback and is retained as a
separate test setup failure, alongside the corrected causal failure.

A dedicated owned monitor now supervises the same child independently of host
callbacks. It checks absolute root deadline, atomic task validity, sampled RSS,
32-MiB report and ten-byte progress caps every five milliseconds. It terminates
and waits on the actual process handle. One mutex protects concurrent process
operations, peak observation and latched failure. No live Print/Model/UI access
or external callback enters the monitor. Destruction wakes and joins it before
child/workspace cleanup; thread creation failure also cleans up the child.

Host checks around callbacks and final diagnostic adoption propagate the latched
failure and retain no partial report. Existing child protocol, source/request/
software binding, OS CPU/file caps, missing-RSS transition, original numeric
limits and all seventeen mandatory checks remain. No proof/export credential,
Verified transition, new export channel or printer action is introduced.

Production/test changes are limited to:
- `src/libslic3r/Nonplanar/NativeAnalysisWorker.cpp`
- `src/libslic3r/Nonplanar/NativeAnalysisWorker.hpp`
- `tests/nonplanar/test_policy.cpp`
- `tests/nonplanar/worker_probe.cpp`

## Qualified final evidence

Frozen evidence: `evidence/B14-native-watchdog/source-manifest.json` and
`commands.json`; logs/large files are losslessly gzipped. Platform: macOS ARM64,
AppleClang 21.0.0.21000101, CMake 4.3.1, Ninja Multi-Config, Release. Source and
binaries are checksum bound before commit; delivery verifier checks Git membership.

- Final build `build3`: exit 0, 40.960 s. Unchanged build: exit 0, 15.279 s,
  no C++ compile/link. Initial test-only Catch logical-expression compile error,
  short-launch red and corrected live-PID red failures are retained.
- Focus seed 1835281160 with NoAssertions: 13 cases /9320 assertions, exit 0,
  29.685 s. This includes actual production worker and serialized GUI adoption.
- Exact selected CTest discovery/execution: 471/471, zero failure/skip/disabled,
  exit 0, 146.516 s. Other upstream suites are not newly qualified.
- Six PID probe scenarios: deadline, RSS, report size, progress size and stale
  owner all stop inside the blocked callback. Healthy process remains alive in
  that callback and stops only on explicit cancellation after return. Each PID
  is absent after the API returns; measurements are in
  `final-jobs/native-watchdog-observations.json`. Probe is never packaged.
- Fifteen independent inventory/job/lineage/final-byte/worker/view oracle commands
  exit 0. All original 2098 default movements match the separate native owner.
- Ten fresh actual main Orca CLI cases (including refusal/SIGINT) exit 0.
- Six fresh original OFF/ZAA comparisons pass with original strict normalization
  and the existing OFF-default allowance only. No golden/model fixture changed.
- Fifty of 51 parent candidate artifacts remain byte exact; only software-bound
  `native-job-report.json` changes. Geometry/material/candidate outputs retain
  their original bytes, margins, refusals and contact exceptions.
- Inventory: 18,300 files, 2,435,409 bytes, SHA256
  `36f0b3dae89eefa6068bda802712260121ac211aefb1d77384c98453d150cbc6`.
- Default candidate: 2098 movements, SHA256
  `c330363d909d5907a52b151a7ca499d1ce72681cc0185edd6f9c312bef12686b`.
- Fresh isolated `b14-native-watchdog-delivery` bundle contains the same actual
  worker. Network/write/datadir isolation probes pass. Main binary SHA256
  `a180efd4b41b30bf3663f51e9d61f9752f3d470d5e2e7b267122e9ce3855c62b`;
  worker `06f210d3c5271bcdf583f8cd99553ae4b9a25364c32927dc70d7e2bc93f37463`.
  Native GUI runtime was not rerun for this source change; previous minimal
  synthetic GUI observations remain in B14-native-gui.md. Full GUI/P2 remains open.
- Pinned-source and original normative package audits exit 0. Fixed report stays
  **4 PASS/RUN +13 UNKNOWN/NOT_RUN**, overall UNKNOWN and export BLOCK.

Exact final command argv/exit codes are in commands.json. Reproduction:

```sh
cmake --build build/arm64 --config Release --target nonplanar_analysis_worker nonplanar_tests fff_print_tests nonplanar_rate_audit OrcaSlicer -j6
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[NativeAnalysis],[NativeAnalysisJson],[NativeAnalysisWorker],[NativeAnalysisView]' --rng-seed 1835281160 --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir <fresh-evidence> --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/native_worker_oracle.py --fixtures <fresh-job-fixtures>
python3 scripts/nonplanar/native_view_oracle.py --fixtures <fresh-job-fixtures>
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 <fresh-baselines> --allow-nptop-off-default
```

## Remaining required work

A blocked host callback still prevents API return until it returns. Cancellation
predicates remain on the host thread; the monitor independently checks only
atomic task validity. Host input capture, private Print.apply and JSON
construction/parsing are not hard preemptible or host-RSS contained. RSS is
sampled; memory can overshoot before observation, so this is not hard macOS
memory containment. OS observation/termination calls, descendant processes and
parent-crash cleanup are not newly qualified. Cached build controls, including
GIT_COMMIT_HASH=a77fb582, are recorded as compiled; source hashes are actual,
while complete software identity remains UNKNOWN/NOT_RUN.

Linux new-head CI and Windows runtime, independent review and physical tests
remain pending. Continue hard parent/child containment, complete import/source/
3MF, domain editing/visualization, full cap/later passes, qualified contact/seams,
whole-job/order/geometry, all seventeen domains and atomic publication. Standard
U1 head and 0.4-mm nozzle are known; software work proceeds independently.
