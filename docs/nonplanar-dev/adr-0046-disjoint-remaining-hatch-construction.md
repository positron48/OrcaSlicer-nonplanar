# ADR-0046: Construct remaining first-hatch paths with certified nominal fill gain

Status: bounded implementation; independent safety review pending.

## Invariant

Construct replacement intervals of one original first hatch in space disjoint
from the actually laid cap's D_upper XY projection. Accept only after continuous
finite-width roof/amount proofs and a new nominal union/fill measurement certify
positive target coverage under the declared outside-volume limit.

## Decision

`plan_remaining_first_hatch` takes protected hatch/fill snapshots and requires
the same actual body prefix and first target. Exact convex clipping of every
completed/current D_upper footprint supplies blocked longitudinal intervals.
Future records are absent. Add the requested XY separation, merge the intervals
and round candidate endpoints inward; charge affine Z conversion error. Each
candidate reuses the finite-width first-hatch solver. All candidates share the
whole-line amount, packet/roof counts, source-walk/query work and deadline limits.

Capture only the proposed additions in a fresh declared ledger; connector travels
are unqualified. Integrate its nominal occupied union and reconcile it against the
same owned target. New nominal footprints are disjoint from every old D_upper
projection, hence old/new nominal sets are disjoint. Their covered/outside measures
can be added with outward bounds. Preserve the original current prefix exactly;
do not freeze its nonrepresentable butt into rewritten binary64 records or invent
a single-prefix union certificate for the two sets. The protected result retains
both measured sources, selected policy, paths and prospective combined measures.

These candidates replace unlaid intervals of the selected line. They must not be
printed in addition to that line's original future events. They are geometric
proposals, not a selected/executable whole-job order.

Internal first-hatch contract 3 retains explicit solved `path_start/path_end`,
including subintervals; original factories still solve their complete source line.
Remaining-hatch contract is 1. No current persistence/cache/project/profile/IR
encoding contains these snapshots, so external migration is unnecessary. Every
existing consumer remains diagnostic and the public gate stays shut.

## Boundary

This handles XY-clear first-pass windows, including a current suffix and windows
around an existing middle obstacle. It does not fill remaining vertical voids
under already occupied XY footprints. Global replanning, complete edges/seams,
rounded side-floor contact, actual later support, head CCD, motion/flow, order,
final-byte replay and export remain required. Full B01-B15 remains active.

Analytical current/future, both-axis, transverse-neighbour, shared-limit and
outside-volume negatives plus actual native fill-gain evidence are recorded in
`B07-remainder-hatch.md`. Linux exposed an inherited 20-second fill deadline
failure at 84a709ef; do not loosen that budget to pass. Tighten admitted geometric
bounds separately and verify Linux before claiming this chain passes there.
