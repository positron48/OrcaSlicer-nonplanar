# ADR-0092: isolated native analysis from exact owned host inputs

Status: accepted for bounded worker transport/supervision. Full B14 remains open.

Interactive CGAL/Print work must execute in a separate native process before a
GUI exposes it. The host captures a current Analyzing task and a request already
bound into that job; the request's six actual typed resources must match the job.
The private worker input factory owns its immutable payload. Host job ID,
revision, attempt, fingerprint, request/source hashes and compiled software
inventory identify that payload. No editable JSON can construct a host token.

The child receives exact source config canonical values, raw IEEE double matrix/
coordinate bits, original float32 meshes and properties, material/object/volume/
layer overrides, instances, source metadata and all four annotation data streams.
It rebuilds the existing Model/DynamicPrintConfig, recaptures the entire original
source canonical JSON/hash before Print.apply and runs the common NativeAnalysis
backend with production defaults. It does not resolve presets or reread paths.
Unknown actual setting keys and unsupported config representations refuse.
Referenced volume material IDs also refuse: pinned Print.apply does not copy
Model materials into its internal model before region configuration. Unreferenced
material metadata still round-trips; no shared upstream workaround is introduced.
Canonical v1 distinguishes native scalar enums from generic dictionary enums;
a private enum option preserves that representation and integer meaning without
changing global Orca types, project formats or the mathematical contract.

The host launches an explicitly trusted absolute executable with direct argv,
private workspace, bounded input/output and a 5 ms supervisory poll. Actual
monotone stages use ten single-byte stderr records. Cancellation, stale owner,
root wall deadline, child failure, missing memory observation and malformed/
mismatched response suppress diagnostic publication and reap the same child.
Callbacks must be short, nonblocking background status operations. The host
must check its actual current Print/task again on the serialized GUI thread
before displaying a result or stopping the host attempt. Background code has
no authority to advance host phases or restore Verified.

POSIX CPU/core/output-file limits and Windows process CPU limits apply to the
child. Live RSS and the OS peak are checked against the requested budget, with
64 KiB peak-reporting allowance. macOS rejects the tested RLIMIT_AS cap; this is
observed/supervised memory control, not instantaneous hard RSS containment.
Supervision does not sandbox network/filesystem access: only the exact native
path and inputs are used; GUI integration must apply its own isolated launcher.

The response is display data only. Host and payload identities, report/manifest
hashes, child source/job binding, fixed17 registry and ordered finite movement
fields are checked. All unimplemented checks remain UNKNOWN/NOT_RUN; every
response remains export BLOCK. No candidate bytes or proof/export credential
cross this boundary. A valid incomplete analysis may publish its refusal, with
no partial replay. The worker cannot qualify source geometry, materials,
firmware, physical scene or software by declaring their identities.

The executable/API and real process tests are the bounded delivery. Minimal
GUI, trusted runtime packaging/launch policy, hard RSS containment, full import
and native 3MF/publication remain pending. CLI retains its existing cooperative
execution path. Evidence and author critical review are recorded separately in
B14-native-worker.md and B14-native-worker-review.md; independent review and
Linux/Windows/full GUI/physical execution are not inferred from macOS tests.
