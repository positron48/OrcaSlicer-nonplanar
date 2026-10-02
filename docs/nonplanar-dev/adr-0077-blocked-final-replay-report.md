# ADR-0077: protected final replay report with a fixed mandatory registry

Status: bounded implementation; separate author audit; independent review pending.
Normative DATA_CONTRACTS §§6–7, SPEC §§22/26–27/33–35 and B13 are unchanged.

## Decision

Extend the existing JobArtifact boundary with a private immutable report factory.
The worker owns the binding, Verifying token, material policy and limits before
callbacks. It reads no live Print/Model. It recomputes actual candidate, manifest,
job and policy hashes; independently checks all thirteen manifest fields against
protected inputs, including exact binary64 initial position and uint64 IDs; then
runs the independent final-byte rate and material verifiers afresh. The latter
performs its own rate replay. Both actual executions are charged cumulatively,
without charging the inherited candidate work twice.

One root deadline is positive and at most 1000 ms. Revocation, cancellation,
source/scene/policy freshness and numeric-environment refusal latch for the whole
call. A lower helper catching an exception cannot restore a valid result. There
is a final guard after allocation. Root work exhaustion, cancellation, stale
tokens and numeric refusal return no report. Work/deadline control is cooperative,
not hard containment of hashing, allocation or callbacks.

Registry version 1 has seventeen fixed mandatory IDs, in alphabetical order:
actual_support, candidate_manifest_identity, complete_route_order,
config_compatibility, continuous_geometry, final_filters, final_material,
final_rates, firmware_preconditions, independent_replay,
material_delivery_qualification, numeric_algorithm_qualification,
profile_complete, scene_coverage, software_identity, source_model_plan_binding,
target_volume_seam. The caller cannot provide PASS values or a smaller registry.
Every missing domain is UNKNOWN/NOT_RUN. An actual refused material check remains
RUN/FAIL or RUN/UNKNOWN; dependent material work is SKIPPED if rates refuse.

The four implemented entries describe manifest identity and the declared linear
grammar/rates/dose/journal reconstruction only. Independent replay does not claim
full geometry, complete route, measured profiles, actual delivered material or
firmware transforms. Thirteen other domains remain mandatory and unimplemented.
Any FAIL yields overall FAIL; otherwise incomplete proof yields UNKNOWN. Export
is BLOCK. No Verified phase or certificate is added.

The canonical report wrapper and registry are internal version 1, scope
`declared_linear_final_byte_replay_incomplete_job`. The wrapper binds attempt,
job fingerprint and manifest hash, actual replay statuses/count/work and a
`validation` object in the immutable 0.1.0-draft format. The report records actual
candidate SHA and every mandatory check. It never supplies its own verifier
parameters. Existing native/mathematical/profile/3MF formats are unchanged.

Serialized host admission accepts only this protected blocked diagnostic. It
recaptures native context through the existing exact owner/token admission and
ends Failed or Unknown. Foreign, old, repeated or changed-source results cannot
alter another attempt. Report bytes alone are not a current approval. No files,
publication marker, cache certificate or export route are published.

## Provenance diagnostic

Optional native test traces localize the earlier exact source identity difference
to the derived CGAL body representation: vertex indices 16/17 exchange, with all
forty oriented faces updated accordingly. The twenty-two actual vertices and
oriented triangles match after the explicit diagnostic remap; original/cap/
reservation meshes and source/executed configs remain exact. This does not
normalize runtime hashes or qualify deterministic CGAL output. Exact native
fingerprints continue to distinguish the actual indexed representations. Full
source-to-plan and cross-platform qualification remain open.

## Consequences

Native five-record and complete 2218-record software candidates execute this path.
The independent standard-library Python oracle checks exact report/manifest/byte
identity and the fixed complete registry; it cannot prove motion mathematics,
physical geometry, authenticity or export readiness. Linux CI exercises all
three analytical report outcomes and the complete native report.

Full B13 report semantics, qualified native/resource/software linkage, all missing
checks, Verified, atomic publication/recovery/copied bytes and every route remain
pending. Defaults, losses, margins, original negative tests and OFF/ZAA baselines
are unchanged. No user configuration, printer or physical operation is performed.
