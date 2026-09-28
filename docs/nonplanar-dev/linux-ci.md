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
