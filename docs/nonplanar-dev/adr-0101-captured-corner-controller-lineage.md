# ADR-0101: captured prospective corner repair in the native controller

Date: 2026-10-03. Status: restricted implementation; independent safety review pending.
Parent: 55755955712b3b1fadd346cd35eaf8d6a02ea56b. Follows ADR-0100.

The retained corner factory must be selected through an immutable request before
its actual material can reach motion planning and final-byte replay. Add optional
`corner_replan` with three explicit mm3 quantities: minimum covered gain, maximum
complete outside-target volume, maximum nominal repeated-material increase.
There is no default overlap allowance or operator-confirmed calibration.

Unselected requests retain exact schema1 or schema2 bytes. Selected requests use
schema3 with `corner_replan` and `later_paths` (possibly empty). All three policy
bits, original contour/fill recipe and ordered later program enter canonical
request/job/worker/view identities. Parsing checks exact registries, finite
numbers and policy domains; schema3 without its repair program refuses. Older
readers reject the new schema. The established `native-analysis-request-v1`
resource name remains; its contents declare the actual version. No global
profile/3MF migration or new progress stage is introduced.

Inside the existing Cap stage, create the original cap and run its prospective
repair. Share one original cap work/cell/time budget across both operations;
subtract completed work/cells and round elapsed milliseconds upward before
passing remaining child deadlines. Existing precision, finite-width band,
actual original-body roof, complete nominal spill/overlap and refusal semantics
remain. Reconstruct actual material from the repaired cap, then optional later
paths from that exact first prefix. Motion and final-byte checks consume this
actual journal. Cancellation/staleness/refusal publishes no partial result.

Private native plan4 owns the protected repair, cap-only before/after hashes,
original recipe, explicit policy and bounded gains. Binding requires the exact
repair after-owner and original hatch parent; matching bytes from an independent
copy do not replace those owners. Omitted repairs and changed policy/recipe
refuse. Optional later lineage retains the repaired cap. Corner repair plus the
existing departure is unsupported until that route is constructed for the actual
repaired assembly. Existing native plans/manifest versions remain unchanged.

Selected corner candidates use manifest5 and
`owned_native_body_corner_cap_candidate_lineage_only`, including when later
paths are present. Their native hash includes both lineages. Compiled source
identity invalidates prior software-bound results. Parser/replay and independent
tools check actual final decimal XYZ/E, retained doses/sections, original body,
exact prefix ordering and child/direct linkage. They do not independently
certify arbitrary sloped union geometry or a complete filled target.

The explicit controller fixture uses [.001,.001,.1] mm3 simulation policy;
ADR-0100's separate narrower-cap fixture retains its .05 overlap ceiling.
Neither allowance is physical calibration. The fixed17 registry remains
4 PASS/RUN +13 UNKNOWN/NOT_RUN, overall UNKNOWN, export BLOCK. Complete target
fill, closed seam/connectors/contact/head/routes/order/whole-job/source/software,
cross-platform runtime, physical qualification and publication remain open.
