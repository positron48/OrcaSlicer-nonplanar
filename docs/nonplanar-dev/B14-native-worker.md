# B14: isolated native analysis worker and exact owned source transport

NativeAnalysisWorker now launches a separate native executable from an owned
current host task/request. The child reconstructs existing Model/Print/config,
recaptures exact original source identity and invokes the common backend with
production defaults. Models/presets are not reread. Private payload identities
bind host ID/revision/attempt/fingerprint, source/request and compiled inventory.
Matrices, original float32 meshes/properties, overrides, instances, metadata and
all four annotation streams retain exact bits/order. Host capture uses its
already owned publishable canonical identity and original mesh/annotation data,
without another full geometry/config clone or dictionary serialization.

Direct argv, private workspace, 32 MiB input/output bounds, ten actual stage
records, root deadline, cancellation, stale attempts, live RSS, terminal OS peak,
child failures and strict response parsing provide background supervision.
CPU/core/file limits apply on POSIX; Windows uses a process CPU job limit.
Result adoption remains the serialized host caller's responsibility. Worker
output is display JSON, never proof/candidate bytes or an export credential.
ADR-0092 and B14-native-worker-review.md define the invariant and limitations.

macOS ARM64 / Apple Clang 21.0.0 / Release verification:

- All five targets build, including nonplanar_analysis_worker and main Orca.
  Final source build7 exits 0 / 56.778 s; unchanged build8 exits 0 / 14.521 s,
  with no compile/link. Compiled inventory: 18296 files / 2434951 bytes / SHA
  552db844e3b3e26d3c375c0dde96610f738fdd3bab6e3accdab937956b88f4b7.
- Ten NativeAnalysis/NativeAnalysisJson/NativeAnalysisWorker cases pass 9117
  assertions, seed 1648739313, NoAssertions enabled. Real child analysis uses
  an unavailable source path and matches every movement of a separate direct
  native execution. Caller mutation cannot alter the captured request. Actual
  body-stage and final-stage cancellation, Print.apply edits, newer jobs,
  standard/nonstandard callback exceptions, memory budget, unqualified units,
  malformed/hash/mesh/config/annotation inputs, hangs, signals and failed or
  malformed children expose no candidate/partial replay. Nonzero plate,
  signed-zero/subnormal matrices, override and annotation metadata round-trip.
- Selected CTest executes 468/468, zero failures/skips, gate 128.622 s. Original
  collision/contact/support/material/timeout negatives remain. Exact discovery,
  names, JUnit and actual job/candidate outputs are preserved.
- Fourteen final independent oracle commands pass. The worker oracle rebuilds
  original mesh binary/annotation framing, verifies exact source/request/job
  linkage, scans the actual direct reference's final decimal G-code and compares
  every returned movement/range. Five mutated worker inputs/outputs refuse.
  The original editing oracle and actual native lineage/reports remain verified.
- Main Orca CLI gate passes ten cases / 2.997 s in fresh offline datadirs,
  including actual body-stage SIGINT. Worker/direct/default CLI have 2098 records,
  candidate SHA c330363d909d5907a52b151a7ca499d1ce72681cc0185edd6f9c312bef12686b.
  Original finer 2197-record controller and supported 2205-record candidate
  b8eb15681127c1a8db57ce0aa257326763f16832b82fdafd13ed091c4bd98ef8 remain.
- Fifty of 51 parent candidate files are byte-exact; only the software-bound
  report changes. All six fresh strict OFF/ZAA comparisons pass, retaining
  only the original timestamp and exact nptop_mode OFF default allowances.
  Source audit, unchanged normative bundle and workflow YAML checks pass.
  Linux CI builds the real worker through the test dependency and runs its new
  oracle; execution of this new head is pending after push.

Intermediate failures are retained: initial test build setup/assertion syntax,
incorrect manifest hash encoding, an unsupported referenced-material fixture,
and macOS task-info/exit transition. Hash comparison now follows the actual
manifest's hex representation. The pinned Print.apply does not copy referenced
Model materials before region config, so nonempty volume material IDs refuse
explicitly without changing shared stock Print behavior. Unreferenced material
metadata still round-trips. A persistent missing RSS observation refuses after
at most a 20 ms exit-transition window; OS peak remains mandatory. Callback
exceptions are translated without exposing their supplied text.

A dedicated macOS probe attempts a 64 MiB RLIMIT_AS and a 128 MiB mapping:
setrlimit=-1, mmap_failed=0, errno=22. RSS is sampled and checked against the
512 MiB default, including OS peak plus 64 KiB reporting allowance; this is not
hard instantaneous memory containment. CPU/file/wall controls are separate.
Full 200K interactive work, callback preemption, sandbox policy and trusted GUI
runtime packaging remain pending. The API has no GUI caller yet; the main CLI
retains its prior cooperative path. No printers, user presets or cloud are used.

Exact commands/exit codes, source/dependency/binary hashes, fresh tests, oracles
and lossless baselines are frozen under evidence/B14-native-worker/. Commit
resolves with git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B14-native-worker.md.

Full B01–B15 stays IN_PROGRESS. Fixed17 remains 4 PASS/RUN +13 UNKNOWN/NOT_RUN,
overall UNKNOWN and export BLOCK. Next: minimal native GUI with serialized
adoption/cancel/replay, isolated launch policy and runtime packaging. Hard RSS,
complete import/native 3MF/CLI operation matrix, full cap/later passes, qualified
contact/seams/whole-job order/geometry, source/software/physical qualification
and publication remain open. Linux new-head, Windows, full GUI/physical execution
and independent safety review are NOT_RUN/pending. Standard U1 head and 0.4 mm
nozzle are known; software work remains independent of firmware/material brand.
