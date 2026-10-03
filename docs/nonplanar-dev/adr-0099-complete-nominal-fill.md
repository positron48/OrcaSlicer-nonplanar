# ADR-0099: complete nominal material volume around the original target box

Status: implemented in the restricted C++ model; independent safety review pending.

The protected local `MaterialFillSnapshot` measures target coverage, missing
material and Z spill inside its original box. That box matches the original
footprint and encloses the entire target, but may clip away nominal material
outside XY. A complete target/excess account must retain that local proof and
measure the rest of the same actual prefix. It cannot infer full fill from
summed dose, nominal roofs, Upper geometry or future rows.

`measure_complete_material_fill` captures the local owner and limits before any
callback. It bounds the active nominal finite bead geometry using constant flux,
affine gap and actual current front; endpoint bottom/top bounds and projected
transverse width enclose each bead. The original box and this enclosure form an
outer box. Six exterior slabs, with previous axes restricted to the original
box, partition its complement with disjoint interiors. Shared faces have zero
3D measure. Each slab reuses the existing certified union integrator on the
same immutable actual prefix. Sum U, S and R across this partition; do not sum
pairwise intersections. Complete target spill is local Z spill plus exterior U.
Original target C/M and target/body owners are unchanged.

All exterior integrations share the original remaining work/cells/deadline,
current-revision and cancellation guards. Each child gets one sixteenth of the
remaining interval width, leaving space for outward sums. Every final reported
width must satisfy the original precision. Invalid input, insufficient work,
cell/depth/precision/time exhaustion, stale/cancelled/throwing callbacks or
arithmetic-environment changes publish no complete proof. The measurement
charges new work; the owning candidate also charges its preceding local proof.

Complete-fill contract 1 is private and immutable. First-cap contract 4 retains
that proof beside its original local fill. First cap, end/width replacements,
contour and hatch candidate measurements invoke it. Original spill policies
consume complete spill. Existing source/packet geometry and quantities remain;
no additional paths or policy tolerance changes are introduced. All private
first-cap constructor consumers are rebuilt. No persisted profile/3MF or public
request schema contains this proof; request1/2, native plan2/3 and manifest3/4
remain. Compiled input identity invalidates previous software-bound results.

This proves complete nominal measures for the supplied actual prefix, not an
acceptable deficit or excess, seam repair, real delivery, bonding, tool access,
normal support, full filled cap, whole-job compliance or export permission.
Original native caps remain incomplete and retain all refusals. Next use these
measures with the existing under-material/clear deficit partition to construct
qualified remaining cap volume, seams and subsequent whole layers.
