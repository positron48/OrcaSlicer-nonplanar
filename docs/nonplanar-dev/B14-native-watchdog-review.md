# B14 native process watchdog: author critical review

Date: 2026-10-03. This is author review, not independent safety qualification.

- The monitor owns no callback or live Print/Model/UI reference. Its shared task
  carries an immutable snapshot and an atomic validity flag. Model replacement
  in the callback can invalidate the child without a concurrent model read.
- All concurrent running/exit/termination/observation operations use one mutex.
  Destructor cleanup occurs after monitor join. No process-global SIGCHLD
  handler or deprecated Boost timer wait is introduced. The pinned Boost
  terminate implementation waits for the same PID on POSIX.
- Failure is latched before termination, and the host checks it before terminal
  exit status/adoption. Callback exceptions and stale/cancel/deadline failures
  never expose partial diagnostics. A terminal child still needs its original
  strict identity/schema/stage/mandatory-report and OS peak checks.
- Real PID liveness is checked inside the blocked callback, before supervision
  can resume on the host. Healthy and actual production positives prevent a
  reject-all implementation from satisfying the test. Both output roles and
  memory observation are separately exercised; probe code is not packaged.
- The first red test had an overly short launch allowance, then a corrected red
  test observed the live child after its deadline. A Catch logical-expression
  compile error is retained. These are distinct from the qualified final run.
- Sampled RSS and OS syscalls do not prove hard resource containment. A blocked
  host callback still prevents API return, and its cancellation predicate is
  not moved onto an undocumented thread. Input marshalling/private Print.apply,
  parent memory, descendant/crash containment and full platform runtime remain
  open. Fixed17 still contains thirteen UNKNOWN/NOT_RUN checks and export BLOCK.

Final source, test command/results, platform and remaining scope are recorded in
B14-native-watchdog.md and its frozen source manifest. Independent safety review
is pending; no physical or whole-job qualification is inferred.
