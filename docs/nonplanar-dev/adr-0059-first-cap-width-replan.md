# ADR-0059: Reduce first-cap excess while retaining contour and end packets

Status: locally verified; independent review pending.

## Invariant

Replace only prospective central infill packets between the original contour
centre planes with a strictly narrower nominal width. Preserve every XYZ/gap
approximant, contour snapshot, extended end packet/dose, original actual body,
target, ROI, boundary band and material losses. Rebuild the whole ordered ledger
and require outward positive S/R reductions, bounded C loss/M increase and the
original spill ceiling. Never edit already deposited material.

## Decision

Each old whole-strip roof interval holds at every point of its finite footprint.
It therefore remains a roof bound for a narrower contained footprint at unchanged
XYZ/gap. Check its inherited gap/width limits and prove the new maximum actual
width, including old gap uncertainty, fits inside the old admitted domain. Keep
the inherited roof/gap/numerical proof; do not substitute a centreline query.

Reuse constant-flux fixed-width packet planning with the original endpoints,
affine gap and rounded section. Require one packet with exact old XYZ/gaps; a
required subdivision refuses this construction. Charge the original packet
coordinate error. Recompute each path's mixed-width target integral and actual
commanded amounts, including retained ends; propagate inherited gap uncertainty
and enforce the original global volume error. A second central width replacement
refuses rather than inheriting an unqualified wider domain.

S and R must both decrease by at least the declared minimum. C loss and M increase
are bounded by the same declared tolerance, default .001 mm3, and spill by the
minimum of the original and new ceilings. These are prospective candidate-quality
limits, without allowable-excess or physical-contact approval. Failed geometric,
budget, deadline, revision, cancellation or rounding checks publish no snapshot.
All new packet/copy/hash/measurement work shares original root limits and time;
retained packet and roof counts also fit the original nested budgets.

## Qualified union formula

The existing closed-loop union permits aligned unequal primary packet doses
when the actual top/bottom graphs and longitudinal cuts agree exactly. For every
adjacent pair, prove overlapping flat cores at maximum gap and ordered outer
shoulders throughout the gap interval. Then for one primary interval of length L,
centre span d and gaps h0/h1:

    U_primary = (V_first + V_last)/2 + d*L*(h0+h1)/2

Interior doses affect S/R but cannot change the filled centre span. The existing
transverse-end fibre intersection and disjoint central/left/right partition are
retained. Reuse the same adjacency qualification for the parallel-row kernel;
no tolerance admits unequal graphs, separated cores or reordered shoulders.
Unsupported geometries use the original bounded general solver or refuse.

The target-shadow solver reuses finite group envelope faces as split hints,
as the box union solver already does. A midpoint grid can repeatedly straddle
different contour/infill extents. Every child still uses the original continuous
occupancy/roof bounds; faces alone never supply occupancy, and all additional
queries retain shared work/cell/deadline accounting.

## Runtime contract migration

First-cap version 2 -> 3: each owned packet nominal width is authoritative.
`policy.width` preserves the original contour/generation policy, and source
hatch lines preserve their original provenance. Add first-cap-width-replan
version 1 with protected before/after snapshots and measured changes. Original
generation remains uniform. End replan retains these central packets and their
actual doses when invoked afterwards; joins and interface proofs consume the
new ledger. Rebuild application and both native consumers. No persisted cache,
IR, 3MF, profile, default or original normative schema changes.

## Verification and boundaries

Test-first missing API output, historical bounded trials and final evidence are
retained in B07-first-cap-width-replan.md. Independent 113-bit midpoint dose and
analytic integral difference, flat-section rectangle sweeps, exact packet fields
and native whole-run/body boxes verify observed behavior. Excessive narrowing
creates real shoulder voids; the native .30 mm candidate also fails the original
minimum flat-floor interface width. Retain both failures and use a qualified
moderate replacement; do not loosen losses, support or precision to accept them.

Complete allowable repeated material, shoulder/seam, 3D fill, later actual
support, head/contact/order/flow and final-byte job/export integration remain
pending. Nominal path width does not change the user's .4 mm nozzle hardware.
The public guarded export gate remains BLOCK; full B01-B15 stays in progress.
