# B13: bind native jobs to actual owned worker inputs

A guarded job could previously declare resource bytes unrelated to its actual
body material, simulation scene, motion/serializer or replay options. Optional
typed capture now copies those six parameter groups before callbacks, generates
the six corresponding role resources itself and requires protected compiled
software inputs. Caller resources are source-file bytes only. Opaque overrides
are refused; original diagnostic version-1 fixtures remain supported.

Actual body analysis compares the complete body material request. Native plan
capture/candidate binding compare both original and planned head/scene/clearance,
base material model, motion policy and serializer. Typed binding also requires
its protected native lineage. Existing cap producers add calculated coordinate
error; the exact protected journal determines that derived value. Report replay
must use the captured policy IDs/revisions and every tolerance/dose field.
One-bit or same-revision edits produce no matching body/plan/report snapshot.

Every begin invalidates the old attempt before capture. Original array/resource
bounds, one-second root, cancellation and final native freshness stay enforced.
Caller edits in callbacks cannot change the owned inputs. Late/reentrant capture
cannot clear a newer job. Empty and nonstandard callback exceptions now produce
an explicit capture refusal with no partial result. OFF bypass is preserved.

macOS ARM64 / Apple Clang 21.0.0 / Release evidence:

- Four targets built: nonplanar_tests, fff_print_tests, nonplanar_rate_audit and
  OrcaSlicer. Initial complete build exit 0 / 570.036 seconds; final incremental
  build exit 0 / 22.567 seconds. Unchanged build exit 0 / 13.236 seconds, no
  compilation/linking. Inventory observes 18,287 files / 2,433,910 bytes.
- Five new cases / 226 assertions pass, randomized seed 1648739313 with
  NoAssertions enabled. Selected CTest runs 458 / 458, failures/skips 0,
  113.384 seconds including its gate/discovery. Positive native body/cap/
  departure/replay and original geometry/contact/support refusals execute.
- Eleven independent oracle commands pass. Two typed fixture checks reconstruct
  explicit expected binary64/uint64 vectors and motion/serializer hashes in
  Python; each also rejects nine resource/dependency mutations. Existing job,
  manifest, report and compiled inventory checks remain successful.
- Fifty of 51 parent native candidate files are byte-exact. Only the report
  bound to new compiled software bytes changes. The 2218-record base candidate
  keeps its exact bytes, initial pose, journal and policies. Final supported
  deposition remains 2205 records / 128014 bytes / SHA-256
  b8eb15681127c1a8db57ce0aa257326763f16832b82fdafd13ed091c4bd98ef8.
- Six fresh strict OFF/ZAA pairs pass the original comparator and its existing
  timestamp/off-default allowance. Original goldens and normative bundle remain.
- Source/package and workflow YAML checks pass. The two typed identity oracles
  are added to Linux CI. New-head CI result is recorded separately after push.

Exact commands, logs, JUnit discovery/results, all fresh candidate/job records,
source/dependency/binary hashes and lossless baselines are frozen under
`evidence/B13-native-inputs/`. Source changes are bound by source-manifest.json;
commit resolves with `git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B13-native-inputs.md`.
ADR-0089 and the separate author critical review define the invariant.

The resource roles are bounded declarations: Algorithms currently binds the
serializer; Firmware binds synthetic linear motion limits. Reservation, derived
ROI/pass/hatch/assembly choices, qualified contact/seams/full cap fill, whole-job
order/geometry and GUI/CLI/publication are still incomplete. Source/software/
physical qualification is not inferred from identity. Fixed17 remains
4 PASS/RUN +13 UNKNOWN/NOT_RUN; export BLOCK, full B01–B15 IN_PROGRESS.
Analytical CLI matrix was NOT_RUN in this change; its prior evidence is historical.
Windows/full GUI/physical tests and independent review remain NOT_RUN/pending.
Standard U1 head / 0.4 mm nozzle are known; software work continues independently.
