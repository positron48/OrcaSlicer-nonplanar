# ADR-0067: owned continuously checked lifted Travel

Status: bounded simulation contract implemented; whole B09, independent review
and physical qualification remain pending. Scope: ProfileScene and both native
test consumers. Original normative requirements remain unchanged.

## Invariant

Replace one original Travel with vertical exit/lift, level transfer and vertical
descent/entry at an explicitly supplied physical Z. Every nonzero leg, or the
original instant Travel when all points coincide, must pass complete head/static
and actual D_upper checks. A high horizontal ceiling never certifies the exit
or descent. Travel has no deposition contact exception. Unsupported geometry,
uncertain boundaries, stale sources and exhausted budgets publish no route.

The original protected source owns the complete original journal and synthetic
head/scene. Capture source and limits before callbacks. Preserve every other
record's payload, deposited amounts, pressure transitions, poses, metadata and
event ID, in its original order. Only sequence indices after insertion change.
The first replacement leg retains the original Travel ID; additional legs use
IDs above the maximum of the complete original journal, including future rows.
Reject ID overflow and record growth beyond the original limit. An immutable
output-to-original record map makes the replacement explicit.

## Preparation and proof

The copied public material aggregate is raw owned input, not a certificate.
Existing prepare_material_motion revalidates every row, continuity, pressure
state and canonical identity, and reconstructs all derived geometry. Existing
prepare_simulation_motion recaptures the same scene with the original policy,
checking the complete new nozzle poses and potential Upper height against the
scene and omitted-head coverage. Do not feed an already inflated derived policy
back into preparation.

Each leg then uses the existing continuous composed head/static/material checker
on its new original event index. Earlier inserted Travels deposit nothing; the
complete old deposited prefix remains present and original future material stays
absent. Every captured head component and static obstacle participates. Whole
interval leaves accept; interior collision witnesses only refute. Complete owned
leg certificates and a final source check are required for private construction
of SimulationLiftRouteSnapshot. Diagnostics from an incomplete leg are not a
complete route certificate.

Source walks, cloning, both preparations, all pair tests and every successful or
failed material query share one total work allowance and absolute deadline.
Material cells and static-pair counts accumulate across legs. Children receive
only remaining allowances; callbacks continue to check the original material and
scene revisions, cancellation, rounding and deadline, including publication.
No new margin, loss, depth, precision, timeout or work allowance is introduced.

## Versions and remaining scope

simulation_lift_route_version is 1. Existing scene, material-motion, composed-motion,
material model, ledger, Motion IR, profile and 3MF contracts do not change. Changed
Travel geometry and shifted indices change the ordinary ledger fingerprint; old
motion/job certificates cannot qualify the new ledger. There is no persisted
proof cache. This factory certifies only the replacement legs, not every other
motion or a complete candidate file. The unchanged zero-length replacement may
retain its ledger fingerprint but still performs an instant clearance proof.

Height selection, legal contact/access and full cap/order scheduling, all prolog,
parking/wipe/end motions, axis/flow limits and independent final-byte replay remain
required. The full native wedge's exit is UNKNOWN at its original required margin,
despite a proposed transfer height above its complete Upper ceiling. Keep that
refusal; do not shorten the ledger, raise the synthetic tip, suppress a component
or loosen the margin. B01-B15 stays IN_PROGRESS and guarded export stays BLOCK.

2026-10-02 continuation: `B09-annulus-endpoints.md` / ADR-0079 now refute the
original native lift with a strict declared Upper witness at t=0, confirmed by
independent point equations. The earlier UNKNOWN observations above remain
historical evidence; the refusal is stronger, without changing geometry, margins,
limits, ledger or contact rules. Full native route qualification remains pending.
