# ADR-0028: exact affine upper-segment height bounds

Status: implemented and native tests pass; independent review pending.

Extend the existing affine whole-footprint query with endpoint height and XY
gradient intervals, plus the audited reference face and revision. Publish these
only after the swept XY disk is contained in the selected mask and stays strictly
away from every noncoplanar crease. A connected crease-free footprint lies on one
affine sheet even when the containing patch has creases elsewhere.

Find a selected triangle containing the start using exact nominal XY predicates.
Evaluate its plane at both binary64 endpoints with EPECK rational constructions;
CGAL to_interval supplies outward binary64 bounds. The end may be on another
coplanar triangle: extrapolating the reference plane is justified by the prior
whole-footprint proof, not by endpoint membership alone. Endpoint intervals bound
the entire centerline by affine interpolation. Gradient intervals retain the
same exact plane's local definition. All bounds remain nominal and in the
snapshot's input coordinate frame, without import/placement/physical error.

Predefined oracles include z=2+x/8 and z=2+x/3, both endpoint directions and
stationary queries, and a local ramp strip in an otherwise non-affine patch.
Boundary/crease crossings and late invalidation publish neither heights nor
curvature. Generic XY-only containment does not publish height data. No planner,
surface smoothing, collision qualification or export route is introduced.
