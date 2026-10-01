# ADR-0038: finite fixed-width affine hatch candidates

Status: bounded candidate geometry implemented; full B07 and independent review pending.

Use ADR-0037's protected local volume proof to construct finite rectilinear
centerline alternatives on ADR-0036's selected affine surfaces. The invariant is
source ownership, affine Z, nominal whole-capsule containment and conservation
of complete prospective strip volumes. This stage does not authorize extrusion.
Allowed changes are DepositionModel/PlanarBody APIs, their geometric/native tests,
a separate analytic fixture and execution records.

Advance internal affine-pass-stack contract to 2: only the validated factory can
construct a stack; public immutable fields and copying remain available. The
hatch consumer cannot accept an aggregate assembled from unrelated surfaces and
an integral proof. New affine-hatch contract is 1. Neither contract has a
persisted encoding/cache today. Existing IR, material/body fingerprints, profiles,
3MF and export formats are unchanged. Future dependency keys must include both
contracts, the complete source, revision, policies, geometry and numerical limits.

Capture the source, policy and limits before callbacks. Width means nominal XY
width; pitch is an explicit maximum XY spacing. Alternate X/Y between passes,
retaining forward and reverse endpoints without choosing a motion order. Inset
centerlines by half-width, the existing numerical bound and a configured outer
band. For a convex rectangular ROI, endpoint capsule bounds enclose the whole
segment. This proves nominal finite footprint containment, not D_upper, contact
or head clearance. The source adapter separately validates the entire parent
affine cell and reserved-cap containment.

Compute row positions, strip boundaries and affine endpoint heights in eager
exact arithmetic, then bound storage error outward. Verify actual adjacent
stored-row gaps as well as the ideal pitch; count/rounding cannot silently create
a gap above the requested maximum. A straight segment stays on its affine
surface everywhere, so no extra chord subdivision is needed within this bounded
domain. Curved, multi-patch and facet-transition segmentation remain required.

The first pass splits the owned continuous nominal-roof proof. Later prospective
passes integrate each complete strip between their stored affine surfaces using
exact polygon moments. Sum lower/upper endpoints separately and enforce the
whole volume precision against the parent stack. These volumes include the
outer/end regions that no selected bead has yet filled. Four reported rectangles
describe only configured outer bands; they are not a complete finite-bead,
corner, seam or perimeter remainder. Do not turn the complete strip volume into
E or claim dense fill from overlapping nominal centerlines.

Bound total lines to the caller budget (hard maximum 200000), each pass to 4096
lines, and the whole cooperative deadline to at most 30 seconds. First-pass
splitting retains its separate fragment/evaluation/precision limits. Later
geometry is bounded by line count and the whole deadline; these are not one
shared full-job work budget. The native wrapper revalidates the owned actual
body/original target under one outer deadline and polls all nested cancellation
and revision callbacks. Unknown, sparse/thin/domain/precision/work/rounding or
deadline failures carry no candidate snapshot.

The positive native fixture is a new exact 1:16 affine wedge, with twelve
triangles, 3200 mm3 volume and binary32-exact top vertices. It exercises actual
native partitioning, body slicing, material reconstruction and source-bound
planning. The original 5-degree wedge and all original goldens stay unchanged;
this fixture does not broaden the current exact-affine projection domain.

Next allocate finite nominal beads and boundary/perimeter/seam corrections,
then prove actual subsequent support, full-head/contact CCD and legal motion
order. Independent final-byte replay, unified guarded export and complete
B01-B15 integration remain pending. The existing slice/export blocker stays in
force; software evidence cannot confirm operator geometry or printing.
