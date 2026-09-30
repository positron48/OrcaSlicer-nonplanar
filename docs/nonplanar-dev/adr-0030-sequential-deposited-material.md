# ADR-0030: ordered vertical-section material ledger

Status: contract fixed for implementation; independent review pending.

An owned ordered MotionEvent v1 sequence, declared bead sections, source fingerprint,
revision and material model form one immutable ledger. Do not infer ordering from
Z/layer labels. Require contiguous motion positions, unique event IDs, sequential
indices, ready/retracted state continuity and matching retract/unretract amounts.
Travel and pressure restoration never create a bead. The initial pressure state
is Ready in this contract, a declared input rather than measured machine state.

A deposition's straight XY trace has nonzero projected length. Its nozzle top Z
and vertical gap interpolate linearly. G1 interpolates E linearly, so nominal
volume grows as V*t and the vertical cross-sectional area A=V/XY_length is constant.
Width is consequently A/h for rectangular sections and A/h+(1-pi/4)*h for rounded
sections. The MotionEvent width anchors the midpoint gap; the permitted width
range must enclose every section. A varying gap with an impossible fixed width
contract is rejected. There is no 3D slope multiplier. Rounded sections comprise
a central transverse segment of length w-h and a disk of radius h/2, extruded along
XY with butt ends. Require w>h throughout. Native float Flow reconciliation and
original path binding are separate adapters; arbitrary mm3_per_mm is not silently
treated as nominal width/height geometry.

D_nominal retains this section, not an inflated CAD box. D_upper grows and D_lower
erodes the bead separately using explicit outer/inner model assumptions and
numerical errors. These are two-sided set-inclusion assumptions; a Hausdorff or
endpoint error alone does not prove an inner volume. Rounded inner sections keep
their curved shoulders, so a flat-core-only approximation does not invent gaps
through every dense native row. An exhausted inner radius/core yields no guaranteed
support. No physical/operator qualification is inferred from declared assumptions.

At a prefix, include complete previous depositions and only the traversed portion
of the current deposition. Zero current progress creates no material, including
no inflated phantom point. Future events are absent. The current bead and previous
neighbours remain in every representation; contact IDs never remove geometry.
Local contact exception qualification belongs to B10 and cannot be a global mask.

Distinct tagged nominal/upper/lower views prevent a support consumer from receiving
the collision representation by implicit conversion. Membership queries use outward
intervals and return Inside/Outside/Unknown in the declared material model; they
are not collision or support approval. Varying-gap constant-flux lofts need not
be convex; vertex-only coverage cannot approve a footprint. Union coverage, tool
CCD and first-contact localization remain separate.

Bind every event, bead section, derived length/volume interval, model assumption,
source/revision and internal schema 1. Bind a prefix identity to the parent ledger,
completed count, current fraction and cumulative commanded nominal amount. The
amount is not the measure of the bead union when depositions overlap. Use a domain-separated SHA-256 chain of
bounded canonical record encodings, avoiding a multi-megabyte aggregate temporary
for a large sequence. Changing event order/volume/shape/model changes the identity.
No persistent project/IR v1 migration; future consumers must bind this ledger schema.
Fixed aggregate limits and cooperative cancellation/deadline/current-revision
checks reject without accepted partial state. Hard worker containment stays pending.

Positive/negative tests include analytic angled and sloped variable-gap sections,
prefix growth, absent future material, neighbour retention, inner/outer separation,
retract/unretract without phantom beads, volume/state/order mutations, owned inputs,
numerical boundaries and limits. Use independent long-double section/volume oracles
and cross-language hash framing. Native body binding follows before material is
used for any B04/B06 coverage decision. Public guarded export remains blocked.
