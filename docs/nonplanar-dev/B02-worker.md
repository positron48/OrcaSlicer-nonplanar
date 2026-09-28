# B02: isolated native import analysis and revision-bound replies

Invariant: a non-cancellable library call cannot complete a timed-out, cancelled
or stale analysis, and a child reply for different bytes/revision cannot be
accepted. This is a blocking background-job API and a native executable, not a
new service, GUI-thread operation, public export route or complete job verifier.

The parent captures source bytes and SHA-256 before callbacks and owns its limit
values. It writes the bounded snapshot to a private temporary workspace and
launches the trusted native worker directly with argv/stdin redirection. The
executable path is an internal trusted dependency; profiles/models must never
supply it. No shell, imported macro or post-processing command is evaluated.
Source-path/file-descriptor capture and GUI scheduling remain separate work.

The worker invokes the native B02 snapshot importer in a separate process. The
geometry domain remains 2 MiB, 5000 facets, 15000 vertices and 10000 mm; units must
be explicitly declared. It returns protocol 1, revision, SHA-256, geometry status,
face count, volume interval, coordinate-error bound and observed peak RSS. This
summary does not transfer a normalized mesh or install a result into Model.
No printer profile or physical qualification is inferred from it.

The parent polls only its child and checks cancellation, deadline and current
revision before and after reading the reply. The installed Boost marks wait_for
as unreliable and its macOS fallback can alter SIGCHLD/fork a timer; that API is
not used. The child is terminated and reaped on interrupted/error paths. Reports
are at most 4096 bytes, with an exact field set, protocol/revision/hash binding and
finite checked geometry values. Failed launch, nonzero exit/signal, malformed or
oversized output, callback exception and unknown resource evidence fail closed.

Resource semantics are deliberately distinct. Parent wall-time checks and child
termination isolate native calls; they are not hard real-time OS scheduling
guarantees. Child CPU time is capped at 5 seconds; Unix core dumps are disabled
and per-file size is capped at 4 MiB. Peak RSS comes from the OS after bounded
report serialization. Fixed-buffer encoding/write/_Exit follow, with a 64 KiB
reporting allowance charged by the parent. An excessive or unavailable observed
peak rejects the result. This is an observed memory-budget gate, **not an
instantaneous hard RSS containment claim**: macOS RLIMIT_RSS is advisory. A hard
host-memory containment requirement remains open and must not be marked complete.

The worker exits without library shutdown after its reply; temporary native
parser streams have already closed. The parent owns/cleans its input and output
workspace. The worker's standalone stdin interface is internal: supervision by
the parent is required for a wall deadline. No helper is installed into the GUI
bundle yet, and Windows runtime/build behavior has not been executed here.

Tests use the actual native worker for positive analytical input, immutable bytes,
revision, malformed source, units and observed-memory rejection. A separate test
executable (not the production worker) exercises a hang, cancellation/staleness,
exception, nonzero exit, signal termination and malformed/wrong/oversized reports.
Independent review and complete GUI/CLI/3MF job integration remain pending.

## Observed validation

macOS ARM64 Release: worker/probe and both selected native suites build with exit
0. Four worker cases pass 45 assertions with NoAssertions, including actual OS
signal termination. Selected CTest executes 114/114 with no failures/skips. A
separate native worker invocation accepts the 96-face flat fixture, encloses its
analytical 2400 mm3 volume and observes 11911168 bytes peak RSS before the parent's
65536-byte reporting allowance. Exact commands/exits, native XML, raw worker
reply, compiler/platform and source/binary hashes are in evidence/B02-worker.
The normative package audit passes. Report-bound validation compiles without PCH
and fast-math, preserving finite checks independently of stock build flags.

No fresh application/OFF/ZAA run: these new internal modules have no production
caller and shared stock paths are unchanged. The six B02-snapshot captures remain
the last actual baseline comparison. Linux run 36402596420 built app/tests at
6ffcd91f, then failed the selected-test step with exit 2. Its detailed logs/artifact
require unavailable authentication; the failing case is not yet known. Run
36413130735 is still in progress at this checkpoint. Neither is evidence for
this worker implementation. Windows, sanitizers, independent review and physical
qualification remain NOT_RUN/pending.
