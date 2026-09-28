# Native Linux CI evidence job

The dedicated nonplanar-native workflow uses a disposable Ubuntu 24.04 x86_64
runner, pinned checkout/upload actions, read-only repository permissions and
existing pinned build_linux.sh entry points. It builds dependency sources and
native app/tests, then selects both actual test executables via CTest discovery.
Missing executable, empty selection, failure and skipped tests reject the run.

No release/publishing/printer operation or user preset access exists in this job.
Source/package auditing precedes build. Logs, exact commands/exits, platform,
compiler, CMake cache and JUnit/discovery upload even on failure. Build timeouts
remain failures, not evidence of Linux support. The effective run result is
recorded separately; a workflow file or push alone does not count as a pass.

Local target-selection harness validation: 8 Python tests passed using real
CMake/CTest, including missing/empty targets, disabled tests, failures and skips.
These are harness tests, not a native Linux slicer result.

First remote run 36401877948 stopped at the source audit (exit 2); no build
ran. The default shallow checkout cannot satisfy the audit's baseline commit
and ancestry checks. Fetch full history, create evidence before the audit, and
retain its output on failure. Public API annotations confirm the failed step;
raw remote logs require authentication and were not available.

Run 36402596420 (6ffcd91f) subsequently completed both pinned dependency and
native application/test builds, but its selected test step failed with exit 2.
Public annotations contain only that exit code; unauthenticated artifact/log
requests cannot establish the actual failing test. This remains a test failure,
not a Linux pass. The observation and step timestamps are retained in
evidence/B02-worker/linux-ci-observation.json.

Future native test-step failures publish the gate error and up to five failed,
skipped or unbuilt test names with bounded JUnit output in check annotations.
Log text is escaped as workflow-command data, and unreadable/missing reports are
explicit. The original test exit code, selection, acceptance rules and full
uploaded logs are unchanged. This is diagnostic visibility, not a failure fix.
Six diagnostic tests and all eight existing real CMake/CTest harness tests pass
locally; exact commands/exits/output are in evidence/ci-diagnostics.
