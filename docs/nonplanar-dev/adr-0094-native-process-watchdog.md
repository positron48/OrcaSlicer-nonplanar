# ADR-0094: native child supervision independent of host callbacks

Date: 2026-10-03. Status: accepted for the bounded native analysis worker.

## Problem and invariant

SPEC 27 requires interrupted native work to produce UNKNOWN and no exportable
job. The original worker poll invoked the caller's progress callback before its
next deadline/RSS/output check. A callback blocking in the host therefore let
the child continue beyond the root timeout. A regression against the old binary
observed its actual PID still alive inside that callback after the deadline;
refusing output after the callback returned did not satisfy process containment.

The invariant is that native child deadline, sampled RSS, output byte bounds and
atomic task invalidation are supervised independently of progress callbacks.
Any observed failure terminates and waits for that same child. A callback must
return before the synchronous host API can return; this change does not claim
preemption of host code or an instantaneous hard memory limit.

## Decision

Keep the direct-argv trusted executable, existing child protocol, private
workspace, OS CPU/file caps, 30-second upper budget and original RSS caps.
An owned monitor thread receives only the immutable task, absolute deadline,
RSS cap and private output/progress paths. It never invokes external callbacks,
accesses widgets or touches live Print/Model state. The task validity flag is
atomic. A single mutex serializes process handle operations and protects the
first monitor failure and observed peak. The five-millisecond wait releases that
mutex. The old twenty-millisecond missing-RSS exit transition is retained.

The monitor terminates/waits on a deadline, stale task, observed RSS excess,
32-MiB report excess, ten-byte progress excess or observation failure. The host
checks its latched result before/after callbacks and before diagnostic adoption.
Destructor stop/wake/join completes before child cleanup or workspace removal.
Failure during thread creation also cleans up the already spawned child.
Exceptions and failures still clear diagnostics; no token or certificate is
minted. The GUI and worker clients use this same path without API/schema changes.

## Evidence and limits

B14-native-watchdog.md records red/green tests, actual PID observations, native
replay identity, full selected CTest and OFF/ZAA comparisons. Dedicated probes
exercise time, RSS, report/progress size and stale inputs during blocked
callbacks. A healthy probe remains alive inside a bounded callback and is
subsequently cancelled; actual production worker positives remain required.

RSS is sampled, with OS peak checking on a valid final response; transient spikes
can exceed the cap before observation. Host callbacks, input capture, private
Print.apply, JSON construction/parsing and OS observation/termination syscalls
are not hard preemptible. Callback cancellation predicates are still called on
the host thread; only atomic task invalidation is inspected by the monitor.
There is no new descendant-process/crash containment claim. Linux/Windows runtime
and independent review remain separate qualification requirements. All seventeen
mandatory checks and original UNKNOWN/export BLOCK semantics stay unchanged;
full B01-B15 remains active. Complete hard parent/child resource containment,
source/import/3MF, cap/contact/order and final publication remain open.
