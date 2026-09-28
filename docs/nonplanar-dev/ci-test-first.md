# Linux tests before GUI build

Run 36422010850 (042a11ac) exceeded its 120-minute job limit. GitHub's final
annotation explicitly reports the two-hour maximum; the job conclusion is
cancelled. Dependencies completed at 13:37:31 UTC after 60m03s. The combined
application/test build was cancelled at 14:36:44 UTC; selected CTest and CLI
validation never ran. Artifact upload completed, but no test pass or compiler
failure is inferred from this timeout. Earlier test-step failures in runs at
6ffcd91f and 1a2b6c40 remain separately unresolved.

Split the pinned native script's supported phases: -trlL builds tests first,
then the unchanged nonempty selected CTest gate and STL CLI validator run.
Only after those succeed does -srlL build the application, profile validator
and native packaging steps. Both invocations retain Release, Clang, lld, the
same Ninja Multi-Config directory and two build jobs. BUILD_TESTS remains ON
in the existing CMake cache for the second invocation. Failed tests still fail
the job and publish bounded diagnostics; GUI build is skipped after failure.
No acceptance test or assertion is removed or weakened.

The job budget is 180 minutes, allowing the observed dependency/build duration
plus margin. Exact dependency restore/save remains enabled. The workflow itself
is part of that cache identity, so this change intentionally uses a new key;
no old-cache reuse is claimed. Source inspection of build_linux.sh's -s/-t flags
and shared configure block establishes the phase split. Local Ruby YAML parsing
and command/order/budget checks pass, with exact command and output retained in
evidence/CI-test-first. Execution of this revised Linux workflow is pending;
these structural checks do not prove a Linux build or test result.
