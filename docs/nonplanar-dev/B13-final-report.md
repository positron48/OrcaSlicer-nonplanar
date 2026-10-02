# B13 protected blocked final replay report

Invariant: ADR-0077. Scope: protected actual final-byte/manifest replay report,
fixed mandatory-check completeness and stale/foreign host admission. Full B13
and B01–B15 remain IN_PROGRESS; guarded export remains BLOCK.

## Implemented path

`JobArtifact.hpp/cpp` now supplies `verify_guarded_candidate_report` and
`accept_guarded_candidate_report`. The worker captures immutable input/token/
options/limits before callbacks, reads no live Print/Model, recomputes hashes and
checks all actual manifest fields. It then independently replays final bytes for
rates and material under original protected policies/declarations/losses. Both
actual calls are charged cumulatively under one root deadline. Cancellation,
revocation, freshness and numeric-environment errors latch; resource exhaustion
or a final guard refusal returns no report.

The factory owns a fixed 17-ID mandatory registry. Genuine manifest, rates,
material and declared journal replay yield four PASS/RUN entries on the positive
fixtures; thirteen full-job domains remain UNKNOWN/NOT_RUN. Invalid material
policy yields actual RUN/UNKNOWN; an actual nominal dose discrepancy yields
RUN/FAIL and overall FAIL. Incomplete checks cannot become PASS or disappear.
All produced reports forbid export. Successful host admission closes the exact
current attempt as Unknown or Failed and revokes its Verifying token. Old,
foreign, repeated and direct-native-edit results cannot alter a newer attempt.

The canonical version-1 wrapper includes actual candidate/manifest/job/attempt
binding and replay status/count/work. Its nested validation object retains the
unchanged normative draft format. Scope is explicitly
`declared_linear_final_byte_replay_incomplete_job`. No full geometry/support/
complete route/profile/material delivery/firmware/software qualification,
Verified certificate, atomic publication or export is inferred.

## Executed software verification

macOS ARM64, Apple Clang 21.0.0, Release. Final four-target build succeeds in
12.741 s. Seven new report cases cover actual complete registry/outcomes,
callback ownership, foreign/old/repeated/native-edit admission, initial/mid/final
cancellation, callback exception including an empty message/nonstandard exception, scene/source/policy freshness, invalid timeout,
numeric environment, cumulative work-minus-one and real background replacement.
Combined JobContext/JobArtifact/JobReport: 16 cases, 534 assertions, zero failures/
skips, 1.355 s, including optional evidence assertions.

Final exact-index CTest executes all 414 discovered selected tests, zero failures/
skips, 96.716 s. It captures job and complete native report fixtures using the
same two evidence environment variables as the Linux workflow. The independent
Python context/candidate identity oracles and eight report oracle invocations
pass on fresh separate and CTest fixtures. The final report mutation experiment
accepts four actual records (positive, invalid-material UNKNOWN, dose FAIL, full
native) and rejects 34 altered records, including self-rehashed false registry/
status/manifest claims, duplicate keys, NaN, overflow numbers and boolean revision.
This oracle checks identity and registry semantics, not geometry/authenticity.
It uses the standard library and exact current draft shape; the external
jsonschema runner is not required or claimed as executed.

The complete actual native candidate executes job capture, phase transitions,
candidate binding, independent report replay and blocked host admission. It has
2218 records, 2092 depositions, 128890 bytes, SHA256
`d31bc1ef4ec3c4c1342d0ec2ecd89818749acf52a33ce77a1f149d32f32220a8`.
Report work145632, four PASS/RUN and thirteen UNKNOWN/NOT_RUN, overall UNKNOWN,
export BLOCK. Native positive context still uses seven explicit unconfirmed
opaque resources and original named STL bytes; it does not qualify native
source-to-plan equivalence. Original complete native test/support checks pass:
137806 assertions with optional trace/report output, support work691602/cells104.
The final root native run matches all eleven original parent candidate/policy/
query files exactly. Complete native B09 exit remains UNKNOWN.

All 100 original CLI cases pass: rate10/material10/cover18/joined21/Nominal21/
support20. Their original cases/policies remain intact. Six fresh OFF/ZAA captures
match untouched stock snapshots with only the existing timestamp and OFF-default
allowances. Immutable package and pinned-checkout audits pass. JobArtifact still
uses strict floating-point flags without PCH; headless verification links its
independent library without slicer/GUI. No CMake/shared Print/default changes.

## Exact provenance difference

Earlier root and test-directory runs retain the two source fingerprints
`f83a97b3619ee3d1bd25a801de64b37206b70fea6506c1789c315a60c4b5c4e1`
and `11742e75f232c7fbff5c184802dc7db2e8820207ecc51838f09ba449c5d8cf1c`.
New raw ancestry and indexed mesh traces localize the difference to the derived
CGAL body vertex/triangle representation. Vertex IDs 16 and 17 exchange; all
40 oriented triangle rows match after that exact remap of the 22 vertices.
Original/cap/reservation meshes, original source and executed configs remain
exact. Guarded derived input, partition/body/material identities consequently
differ; all final G-code/events and other policy values remain exact.

Two later runs in different directories both emit the second identity; the final
root run emits the first. A working-directory cause is therefore not established.
`native-provenance-difference-final2.json` preserves exact contexts/hashes/remap. Strict
file comparisons retain each original mismatch as a refusal. No runtime native
hash normalization, indexed mesh rewrite or cross-platform/CGAL determinism claim
is introduced. Full provenance qualification must define and verify this boundary.

## Evidence and remaining work

Archive: `evidence/B13-final-report/source-manifest.json`; raw:
`build/nonplanar-evidence/B13-final-report/`. Every logged argv, cwd, exit code,
suite/fixture, source/dependency/binary hash and lossless raw/archive mapping is
retained. Earlier six-case report runs predate the added dose-failure case and
expanded refusals; two initial oracle invocations fail on missing jsonschema.
Author review then found an empty callback exception message could evade the
string-based latch inside a catching helper. The final exception_ptr latch and
empty-message/nonstandard-exception regressions pass; final2 build/focus/native/
CTest/baselines and final3 mutation records identify the final source.
The final standard-library oracle and all final fixtures pass. Those intermediate
records are historical, not final-source qualification. Configured compiler Git
label remains parent `04a8e410`; exact hashes identify the actual build, without
claiming a qualified runtime software resolver. Resolve this checkpoint with:

`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B13-final-report.md`

At 15:57:49 UTC, Linux parent04a8e410 is successful, c2350792 in progress,
9c785290 successful; db7dee0b/41b03282 cancelled. The exact new revision requires
its own CI result. Windows, full GUI/export integration and physical runs NOT_RUN.
Separate author critical review is recorded; independent review remains pending.

Next: implement qualified source-to-plan/plate/resource/software association and
actual complete checks for the remaining mandatory domains, then Verified and
atomic publication/recovery/copied-byte gating across every route. Remaining
B12 geometry/contact/head/cap/route/delivery/transform work and full B14/B15 stay
open. Standard U1 head/.4 mm nozzle is retained; firmware version/material brand
are not prerequisites for continued software work. No user settings or printer
state are touched; no physical qualification is inferred from these fixtures.
