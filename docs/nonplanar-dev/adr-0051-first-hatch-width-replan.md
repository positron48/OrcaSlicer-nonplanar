# ADR-0051: Reduce first-candidate excess through measured width/pitch replanning

Status: locally verified; independent review pending.

## Invariant

Replace the complete prospective first candidate with narrower nominal line
widths and redistributed transverse centres. Preserve the actual body/target,
line count, longitudinal finite extents, original boundary band and later
prospective paths. Recompute whole-footprint gap/amount proofs and the joint
S/U/R/C/M/spill. Require outward positive reductions in both commanded S and
multiplicity excess R, bounded declared coverage loss and outside-target volume.
Never append the old hypothetical cap or label an unresolved deficit filled.

## Decision

For two congruent rounded sections shifted within their actual flat cores,
U = integral(A + h*d). Decreasing width while increasing centre separation
inside the same outer band can retain U while reducing S and R. Beyond that
geometric domain, narrowed paths can create real voids. Therefore no width/pitch
formula alone accepts a replacement: reuse continuous whole-ledger union/fill,
including the general solver, and test measured C change and M increase.
The default maximum loss is 0.001 mm3, a declared candidate-quality tolerance;
it does not alter geometric/safety margins or establish filled/contact approval.

The caller supplies a strictly narrower width. Retain every longitudinal
endpoint, lift new transverse centres onto the original affine first surface
with exact arithmetic and charged storage error, and constrain pitch by both
the original maximum and the new projected width minus source numerical error.
Repartition complete first-strip owners at new midpoints through the original
owned roof proof. These quotas remain distinct from commanded packet amounts.
Recompute aggregate quota intervals; later paths and their widths remain owned
and unchanged. A new protected hatch snapshot and fresh cap ledger retain the
same original stack and target proof. Retain exact old snapshot/ledger separately.

Validate original nested limits before reductions. Charge copying, centres,
strip proof cells/work and whole-candidate reconstruction to root budgets and
original nested deadlines. Cancellation/revision/rounding checks continue until
publication. Refused, partial, stale, clipped, exhausted or timed-out candidates
publish no snapshot.

## Contract migration

Affine hatch version 3 -> 4: `policy.width` records the original generation
policy; each owned `AffineHatchLine.width` is authoritative for its path. Only
protected factories can create snapshots. Ordinary generation still uses one
uniform policy width. First-width replanning changes first lines only, retaining
extent provenance and original policy. Consumers must not re-derive line widths
from that original policy after replanning.

First-bead version 3 -> 4, finite-cell version 1 -> 2, remaining-hatch version
1 -> 2 and complete-first-layer version 1 -> 2 consume owned line widths.
Finite-cell outer bounds use the first/last line radii; first-bead whole-roof,
constant-flux width/amount/error and remaining-space exclusion use their selected
line width. Add first-width-replan version 1. End-replan geometry remains
unchanged and copies the owned line widths. Rebuild all native consumers;
no persisted cache, IR, 3MF or profile representation exists for these snapshots.

## Boundaries and evidence

This is a measured prospective material replacement, not an actual partial-cap
repair, perimeter/seam construction, qualified side/floor contact, head motion,
order/flow, later support or export permission. Material-width changes do not
change the nominal 0.4 mm nozzle hardware. Full B01-B15 remains active and the
public gate stays blocked. No physical evidence is inferred from local/CI tests.

Positive/negative tests precede implementation. Both axes use independent
113-bit actual stadium S/U/R arithmetic; a displaced four-line partition uses
independent circular-lens intersections and preserves a real bounded C loss.
Invalid requests, repeated width, too-narrow domains, excessive C loss,
insufficient reduction, mutable wrappers and shared budgets/callbacks refuse
as appropriate. Native wedge evidence and final commands/XML/hashes are recorded
in B07-first-hatch-width-replan.md after verification.
