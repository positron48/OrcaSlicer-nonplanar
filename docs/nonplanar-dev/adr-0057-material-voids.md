# ADR-0057: Keep under-material voids distinct from vertically clear deficit

Status: locally verified; independent review pending.

## Invariant

For the original protected first-pass target T and actual nominal occupied cap
prefix U within its original XYZ union box, measure its downward vertical
shadow H clipped to T. At each XY point with material in that box, H extends
from the original actual body roof to the lesser of the target surface and the
highest occupied height. Thus C = T intersect U is contained in H, H is contained
in T, and missing target volume is (H minus C) plus (T minus H).

These are nominal geometric measures. Under-material deficit is not a sealed
cavity certificate; vertically clear deficit is not tool access, support,
physical bonding or permission to deposit. Material outside the original union
box is a separate obligation. The actual union must never become an imaginary
solid column underneath its roof.

## Decision

Add protected material-void contract 1 owning its original MaterialFillSnapshot.
Return shadow, under-material missing and vertically clear missing intervals,
with shared cells/work/deadline and publication callbacks. Wrapper fields cannot
replace the source. Preserve every original completed event/current fraction;
future events remain absent. Original fill/union/material/interface contracts,
losses, target, ROI, amounts and numeric errors retain their meanings.

Extend the private union integrator with a distinct target-shadow measure. Only
actually occupied sections intersecting the original XYZ box supply shadows.
Use original finite footprint/current-butt clipping, complete packet-chain
coverage and continuous body roof bounds. Whole vertical AABBs may establish
that a bead needs no Z clipping; they never establish XY occupancy or support.

Use the target's exact affine shear and exact polygon clipping/moments to bound
complete shadow columns. Partial footprints retain union bounds. On a rectangle
covered by exactly one interior-disjoint finite chain, the roof is concave
transversely at each longitudinal point. Minimum with the affine target remains
concave. Only proved positive columns use Hermite-Hadamard endpoint-trapezoid
lower and midpoint upper bounds. Bound every original packet independently;
never average variable flux, fill finite gaps or apply this bound to a maximum
of overlapping profiles. Other shapes retain the general interval bounds.

Reuse identical column queries and polygon moments inside one immutable node.
A parent continuous body roof interval may be inherited by its children when
its width times the whole domain area is at most 1/32 of the requested integral
width. Retain that interval in every volume bound; it is not narrowed or omitted.
Probe samples choose refinement directions only, except the explicitly proved
concavity inequality. Exact full longitudinal containment avoids repeatedly
splitting an already covered finite butt. All limits remain fail-closed.

Allocate the unused caller interval width to the one new integral. Recheck all
final outward differences against the caller's unchanged hard width limit;
rounding can still refuse publication. Clamping uses only proved inclusions
C subset H subset T and the original missing-volume upper bound. No partial
snapshot is published on exhaustion, stale revision, cancellation or rounding
failure. Original default limits remain unchanged.

The complete native cap needs its existing whole-candidate volume policy
(65535 cells, 2000000 work, 5 seconds), with the same .001 mm3 output precision.
Its shorter single-hatch 200000-work policy remains a tested refusal. Variable-
gap analytical positives also declare the complete policy; valid one-cell and
one-depth policies refuse. No existing negative is turned into permission.

No persisted schema, cache, IR, profile, 3MF or public default changes. Rebuild
all consumers and check OFF/ZAA. Remaining 3D fill, permissible excess and void
criteria, complete seam qualification, actual later support, motion/tool paths
and independent review remain separate obligations. Public export stays BLOCK.
