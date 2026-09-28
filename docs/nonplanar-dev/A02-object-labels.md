# A02 follow-up — deterministic object labels

Status: IMPLEMENTED_NATIVE_REGRESSION_PASS, 2026-09-28. Commit resolves with
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/A02-object-labels.md`.
This small stock-path correction was required by the A05 OFF/ZAA comparison.

With object comments enabled and exclusion disabled, GCode read PrintObject's
uninitialized m_id. The member had no initializer and set_object_info assigned
IDs only inside the exclusion branch. These source paths were byte-identical
to pinned upstream before the correction. A05 exposed object ID
72340172838076673 instead of 0; a repeat reproduced this in 3/8 fork runs.
The preserved stock binary matched its original SHA-256, but did not reproduce
the symptom in 8/8 normal-allocation probes. This is source-level diagnosis,
not a claim of an observed failure in that particular stock binary.

Assign the existing sequential IDs before object serialization, whether or not
exclusion commands are emitted. Reuse those IDs in the exclusion metadata. No
new command, configuration default, motion, volume or profile format is added.

The regression poisons both object IDs with 4242 before export and checks actual
start/stop comments contain distinct IDs 0 and 1. Both exclusion states run;
the test also checks the actual EXCLUDE_OBJECT_DEFINE command, excluding mentions
inside settings comments. Before the fix, disabled exclusion deterministically
failed with {4242}; afterwards all eight assertions pass.

Evidence: `evidence/object-labels-red.log`/`.xml` and
`evidence/object-labels-green.log`/`.xml`, with exact command/exit metadata.
Red exit 42; green exit 0. The green command also runs nine A05 cases in the
same worktree (10 cases, 121 assertions), not a pristine intermediate-commit
build. Complete source fingerprints and final shared-path evidence are recorded
with A05. macOS ARM64, Apple Clang 21, Release C++17; other platforms NOT_RUN.

The final A05 run executes 66 CTest cases and compares six OFF/ZAA captures to
the unchanged stock golden files. All six match with timestamp-only
normalization. After the fix, 8/8 additional fork and 8/8 stock label probes
match; the original failed comparison remains archived. The stricter upstream
NoAssertions empty-section issue remains separate and is not waived by this fix.
