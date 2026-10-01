# ADR-0052: Own the first closed ROI contour and measure its corner unions

Status: locally verified; independent review pending.

## Invariant

Construct four connected finite-width edges on the original first affine
surface, preserving the actual body, target proof and original boundary band.
Own the starting corner and direction. Prove every whole footprint before
publishing the complete loop; measure S/U/R/C/M/spill jointly, including corners.
No partial contour, infill strip impersonation or filled/contact/export claim.

## Decision

Reuse the protected first-bead solver and the complete-candidate measurement
from the first-hatch layer. Inset centre lines by half of the declared width
plus its requested width error, the original band and source numerical error.
Lift all four corners with exact affine arithmetic and charge stored-coordinate
error. The same immutable source/target remains owned throughout. Four seam
vertices and both traversal directions are explicit bounded choices. A seam
vertex does not qualify deposited joining material or motion through corners.

Share root and nested path/record/packet/roof/cell/work/error/deadline budgets;
charge source walks and actual amount summation. Preserve active-prefix roof
semantics and absent future material. Reject any failed edge, incomplete nominal
box, exhausted budget, stale revision, cancellation or rounding change through
publication. Measure positive target coverage and enforce the declared outside
volume limit. Connected endpoints need no zero-length travel. Remaining
nonzero connectors retain their existing unqualified travel semantics.

## Closed-loop union domains

The general 3D grid exhausted unchanged limits on useful positive loop fixtures.
Add exact or certified one-dimensional integration only for fully present,
axis-aligned rounded sections entirely inside the original box. Preserve actual
commanded binary amounts, rounded shoulders and orientation. Unsupported,
clipped, unequal, partially current or gapped layouts retain the general solver;
UNKNOWN never becomes approval and no limit or precision is relaxed.

For four constant sections, opposite sides have identical amounts and common
height h. Let A/B be the long/short centre spans and c/e the respective actual
flat-core half widths. Opposite transverse sections must be disjoint. When
B <= 2c, U = V_long + V_short + A*B*h. Otherwise, at half-height coordinate
d in [0,h/2], t = sqrt(d*(h-d)), g = c+t, q = e+t, and the exact XY union area is

    f(d) = A*min(4g, B+2g) + 4qB - 2q*min(B, 2g).

Use outward interval trapezoid lower and midpoint upper bounds only with
certified concavity: c/e >= 0, A >= actual transverse width,
4*(A+B-c-e) >= 4h and A+2c-2e-B >= 0. Before the long strips merge,
f = a0 + a1*t - 4*t^2; the first derivative condition makes f'' <= 0.
At merging, the second condition makes the derivative jump nonpositive;
after merging f is affine in t. Integrate both height halves. Charge every
subdivision/evaluation and retain exact rational accumulated bounds.

For congruent opposite primary packets with contiguous matching affine top,
bottom and gap, require B within their minimum actual flat core. Their union
fills the central span and has U_primary = sum(V_one + B*L*(h0+h1)/2).
Two constant transverse ends must meet the actual endpoints and remain
disjoint. Then U = U_primary + V_end0 + V_end1 - I0 - I1. Each I is an XZ
intersection of an affine strip with a convex stadium, extruded over B.
Bound its fibre length directly with intervals. Where both interval endpoints
prove nonempty intersection, convex-section concavity supplies trapezoid lower
and midpoint upper bounds; intersect these with the direct enclosure. Account
for real end spill, preserve packet discontinuities, and share all original
cells/work/depth/precision/deadlines. Physical 0 <= U <= S tightens valid bounds
without changing geometric margins.

## Contract migration

First-bead version 4 -> 5: line_index becomes optional. Original hatches and
qualified remaining intervals retain their original index; protected contour
edges have no infill owner. Only the qualified contour factory can request
such an edge, with full finite-width roof proof. Its temporary geometry carries
no publishable strip quota. The original affine-hatch snapshot is retained.

Add first-contour version 1. Rebuild all native consumers. No persisted cache,
IR, 3MF or profile representation exists for these internal snapshots; no public
setting/default changes. Union/fill result meaning and original budgets remain
unchanged. Shared measurement additionally charges each actual path/piece sum;
older reported work counts describe their historical revisions.

## Boundaries and evidence

The loop is a separate prospective candidate, not yet combined with the
end/width-replanned infill. It does not repair laid partial cap material, fill
the remaining 3D deficit, qualify contact, joining material, full-head access,
order/flow, curved/later actual support or export. The native loop has positive
remaining M and real positive outside volume; no deficit gain over infill is
claimed. Full B01-B15 stays active and guarded export stays blocked.

Independent 113-bit constant-section circle moments enclose merged, unmerged
and partially merged loops on both axes. An independent circle/line integral
encloses a sloped packet loop with reverse traversal. Mutation/current/gap/clip
negatives, eight seam/direction variants, later-edge ridge rejection and actual
native body amounts are retained. B07-first-contour.md records commands, XML,
failed trials and source/binary/raw hashes after verification.
