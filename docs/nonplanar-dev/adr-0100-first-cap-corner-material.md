# ADR-0100: prospective finite corner material with retained contour owners

Status: implemented in the restricted C++ model; independent safety review pending.

The first cap's contour centrelines terminate at the four original corners.
Finite rounded sections leave corner deficit even after interior hatch end and
central-width replanning. Construct added material on the two contour owners
parallel to the first hatch. Retain every old packet, dose and section, all
other paths and the original target. The two new ends on each selected owner
stay inside the original ROI/boundary band; the perpendicular contours retain
their old ends. This creates a useful candidate for all four corner regions,
without asserting that the entire corner deficit or target has been filled.

`replan_first_cap_corners` captures immutable input/policy/limits before callbacks.
It solves only the four new finite strips against the actual original body
roof, retains old endpoint conversion errors and requires the whole finite
width domain. It measures the complete candidate's nominal U/S/R, target C/M
and spill. Positive covered gain and missing reduction, original spill policy
and an explicit maximum repeated-material increase are mandatory. The overlap
ceiling has no default and is simulation policy, not physical calibration.
No partly deposited cap is replaced in the actual material prefix.

The existing aligned-loop integrator can include wholly exterior packets on
its two outer primary contours. The central graph/section proof is unchanged.
All added packets belong only to the two disjoint exterior child domains at
the original end centrelines. Central volume still removes the old outer half
end sections before summing those complete child unions. Original finite
geometry, dose, deadlines, work/cells/depth and refusal semantics are retained.
The corner measurement allocates half of its final numerical width to initial
union instead of a quarter. Reconciliation independently computes the remaining
width and checks every final interval against the same original precision.
This allocation changes no final tolerance or margin and cannot promote a
failed provisional union to a certificate.

First-cap private contract5 adds `contour_extent`. Existing factories default
to `ClosedLoopCentres`; the new factory publishes `FiniteBandExtensions`.
Old end/width replans refuse that state, preventing extended starts being used
as original corner centres. Repeated corner construction refuses. All private
constructor consumers are rebuilt. Complete-fill witness1 accepts historical
cap4 and current cap5; new corner witness1 requires cap5. Request1/2, native
plan2/3, candidate manifest3/4, profiles and 3MF stay unchanged. Compiled source
identity invalidates software-bound results.

New paths have different endpoints and need new connectors and order. No closed
seam, qualified contact/bonding/head clearance, complete filled cap, target
conformity, whole-job proof or export follows. The common controller still
executes its existing cap; this new replacement is exercised through analytical
and actual native construction tests. Captured request/controller/byte lineage
for selecting this replacement remains the next dependency, followed by seam
routing, under-material/clear deficit construction and full later layers.
