# B13 owned native job context and candidate manifest

Invariant: ADR-0076. Scope: immutable native input/settings/resource ownership,
attempt and phase admission, cancellation/stale handling and exact candidate
manifest association. Full B13 remains open; guarded export remains BLOCK.

## Implemented boundary

`Job.hpp/cpp` captures actual pre-apply inputs and current executed native
Model/config separately, plus full/effective/region/plate settings. Seven opaque
singleton dependency roles and exact named original-file bytes are owned before
callbacks. Private tokens bind the Print owner, monotonically increasing attempt,
source revision, exact context and working phase. Retries invalidate old tokens
even when semantic hashes match. Source changes and Print::clear revoke tokens
before existing cancellation callbacks. Background workers read an atomic token;
host admission also recaptures actual native state. Direct plate/model edits,
tiny config changes, foreign owners and delayed/repeated callbacks cannot revive
an attempt. All native host operations must remain serialized by their caller.

The public job identity uses filtered canonical views, retaining all geometry
bits and unknown overrides. Thirteen exact existing transport/timestamp/log
options are omitted; the nested source fingerprint is replaced by its filtered
identity. Raw captured settings remain exact in memory for freshness, and must
not be persisted as public job data. Different credentials/timestamp/log values
produce equal semantic views at the same source revision; changing them still
cancels an existing attempt. No wildcard or geometry field is silently dropped.
Synthetic raw settings in the software evidence contain only test credentials.

`JobArtifact.hpp/cpp` binds protected final bytes to one current Serializing
attempt. Its manifest hashes actual bytes/size, initial position, original
serializer/motion policies, actual ledger and its source identity/revision,
job ID/hash and attempt. Final cancellation, host freshness and deadline checks
run after the last callback and before returning a Verifying token. A refusal
clears payloads without clearing a newer/foreign attempt. No file is published.

Working phases are not proof reports. The analytical candidate has a separately
declared source chain; the explicit manifest scope is
`job_context_candidate_byte_identity_only`. It does not certify native
model-to-plan equivalence, measured resources, mandatory checks or export.
Version-1 identities are internal new contracts; existing mathematical,
native-source, profile, IR, cache and 3MF formats are unchanged.

## Local verification

macOS ARM64, Apple Clang 21.0.0, Release. Final build of fff_print_tests,
nonplanar_tests, nonplanar_rate_audit and OrcaSlicer succeeds, 42.717 s.
Focused B13: nine cases, 264 assertions, zero failures/skips, 1.065 s. They include
immutable owned data, UTF-8/NUL resources, exact uint64 maximum, reordered resources,
seven individual byte changes, missing/duplicate/unknown/empty/oversize resources,
named source files, current executed model, tiny config/plate edits, mode changes,
every working/terminal transition, retries, reentrant replacement, last-callback
cancellation/edit, invalid durations, foreign owners and real background-thread
revocation after replacement/phase/destruction. Existing guarded process refusal
is retained after positive context/candidate binding.

Independent Python SHA256/JSON/initial-pose rebuilds pass for fresh native job and
candidate fixtures. The final software-only mutation experiment accepts three
positive records, including exact uint64 maximum, and rejects twenty changed
resource/native identity/bytes/pose/policy/journal/source/attempt/manifest cases.
Rehashing a wrong manifest does not make its actual-byte association pass.
The Linux workflow now captures native fixtures and runs both independent oracles.

Final selected CTest executes 407 nonempty discovered tests, zero failures/skips,
99.192 s. The final separate root-directory native run passes in 9.318 s.
The complete original native evidence retains 137783 assertions and all eleven
candidate/policy/query files byte-for-byte against the B12-run-ray archive.
Native G-code remains 128890 bytes, SHA256
`d31bc1ef4ec3c4c1342d0ec2ecd89818749acf52a33ce77a1f149d32f32220a8`,
2218 rows (2092 deposits/126 Travels). Original complete native support still
PASS at work691602/cells104; current standalone all-stage support PASS at
work651623/cells104, .599 s, original one-second root. Nominal terminal and joined
Lower interior PASS; external-front and per-event Lower FAIL, expected exit2.
Complete native B09 exit remains UNKNOWN. All standalone job results stay UNKNOWN
and export false. No older report is reused as current execution evidence.

All 100 original input-preserving CLI cases pass: rates10, material10, cover18,
joined21, Nominal21, support20. Those standalone source/binary/inputs do not change
when the final job-only metadata registry changes. Job and candidate-binding
compilation uses strict floating-point flags without PCH; the independent
headless verifier still links without the slicer/GUI libraries.

Six final fresh OFF/ZAA captures pass against untouched stock snapshots: exact
IDs/commands/replay/resolved settings with the existing timestamp-only and added
OFF-default allowances. Baseline inputs and golden outputs are unchanged.
The immutable normative package and pinned checkout audits pass.

The archive preserves earlier intermediate failures: missing Contracts include,
wrong captured-config overload, old in-flight build graph without JobArtifact
registration, and a native evidence invocation without its required directory.
They are not final-source failures. Initial successful eleven-option identity
tests precede the final thirteen-option timestamp/log exclusion and are retained
as historical checks; final source/build/fixtures/CTest/baselines are explicit.
The initial comparison of CTest-emitted native files refuses on a different
material-policy `source_fingerprint` alone (`11742e75…` instead of `f83a97b3…`);
all events, other policy fields and the other ten files match. The exact origin
of this execution-context provenance difference is not qualified here. It is
retained as a failed identity comparison, never relaxed to PASS. The final
root-directory rerun with the same final source matches all eleven original
files. Whole-job provenance must resolve such differences before certification.
Source/dependency/binary SHA256 and raw/archive byte mappings identify the result;
the configured Git compiler label is the parent `04a8e410`, not a whole-job
software qualification claim. Resolve this report's commit with:

`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B13-job-context.md`

## Remaining and platform evidence

The saved 14:43:57 UTC CI observation has parent 04a8e410 in progress, 9c785290 and
380967dd successful, db7dee0b and 41b03282 cancelled. This new commit needs its own
observed Linux result. Windows, full GUI/export paths and physical tests are
NOT_RUN. The separate critical review is author-only; independent review pending.

Full B13 still needs qualified source/import/model/GUI plate linkage and explicit
compatibility/resource/software resolution, a protected independently replayed
whole-job report and mandatory-check registry, Verified state and atomic
multi-file publication/marker/recovery/copied-byte verification for every file/
CLI/GUI/cache/3MF/removable route. Exact hashes/working phases alone cannot close
these requirements. Full B12 geometry/contact/routes and B14/B15 remain open.
No native or fixture profile becomes operator_confirmed, and no printer/preset/
config/cloud operation occurs. Standard U1 head/.4 mm nozzle remains the user's
declared setup; firmware-version/material-brand questions do not block software.

Evidence: `evidence/B13-job-context/source-manifest.json`, `commands.json`, exact
Catch/CTest IDs, independent oracle fixtures/mutations and full raw logs. The
original normative package is unchanged; full B01–B15 remains IN_PROGRESS.
