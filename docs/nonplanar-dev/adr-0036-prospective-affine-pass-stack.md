# ADR-0036: source-bound prospective affine surfaces and cell volume quotas

Status: bounded internal selector implemented; full B06 and independent review pending.

SPEC section 11 allows vertical offsets of the final surface only after checking
the actual interface, thickness and volume. ADR-0034/0035 now provide those
first-pass material bounds. Introduce internal affine-pass contract 1 without
changing the existing IR, ledger/body encoding, profile or export formats. There
is no persisted selector cache or consumer to migrate. Future job integration
must bind the prefix, source/reservation, target, complete policy and limits; it
must not cache solely by the selected Z values.

For one rectangular affine cell and exactly 2–16 requested passes, preserve the
final target F. Select a strictly interior offset d and prospective surfaces
P_k = F - d*(n-k)/(n-1). Intersect the offset ranges implied by actual first-pass
gap bounds, later vertical spacing and later normal spacing. Do not increase the
number of passes or alter F to hide an infeasible range. A geometry probe at F
may report only a gap failure while retaining valid continuous lower support
and upper roof bounds. Those bounds can derive the range, but do not authorize
a first pass: the chosen P_1 must independently pass the full gap/support and
refined nominal-roof integral query.

Stored intermediate vertices have their rounding error charged to the numerical
budget. Recheck every cell corner, including the affine fourth corner. For lower
and upper plane gradients g0/g1, the normal-ray separation is
h_z*sqrt(1+|g0|^2)/(1+g0 dot g1). Bound this expression with intervals for the
actual stored planes, including source corner error. It reduces to
h_z/sqrt(1+|g|^2) for parallel planes. This is geometric plane separation, not a
proof of support or nozzle contact on future deposited cap beads.

First-pass volume is the bounded integral above the highest actual nominal roof.
Subsequent prospective cell volumes are exact affine vertical integrals between
surfaces, converted to outward intervals. A cell quota is the interval midpoint
only with an explicit positive whole-stack volume error allowance; its error is
recorded. Sum volume bounds outward and check the final allocation against that
single allowance. Quotas are not selected bead volumes, E, swept deposited sets
or perimeter/seam corrections. No slope multiplier is applied twice.

The native wrapper owns the BodyMaterial parent and checks its source/material
fingerprints. It derives F from the owned original mesh through audited upper
analysis and requires an exactly affine patch. Four whole edge capsules with the
source/frame error inset, plus an enclosed-hole check, prove the rectangular
cell belongs to that patch. Physical/BuildPlate conversion and height/gradient
error are explicit; caller-supplied Z cannot replace the source target.

Only bounded convex reservations are supported. Eager exact half-space tests
first prove every reservation vertex lies on the inward side of every face;
nonconvex/degenerate reservations reject. Then outward corner-error boxes of
every selected affine surface must satisfy every half-space. Convexity makes
this a whole-cell containment proof, rather than sampled membership of an
arbitrary solid. Reservation work has a separate explicit counter. Projection,
material work, cancellation/revision/rounding and the original deadline remain
bounded. Capture all handles, requests and callbacks before polling. Hard worker
containment remains pending.

Allowed changes: DepositionModel/PlanarBody internal APIs, their geometric/native
tests and execution records. The complete guarded planner remains blocked.
Stepped-cell subdivision, variable offsets, volumetric seam, subsequent actual
cap support, finite bead allocation, head/contact CCD, legal order, independent
final-byte replay and export are separate remaining requirements. Native evidence
here covers a flat source interior; wider native hole/step/curved cases remain
pending. Existing independent analytic/oracle and canonical-vector tests stay
required. Neither this selector nor its native positive example closes B06.
