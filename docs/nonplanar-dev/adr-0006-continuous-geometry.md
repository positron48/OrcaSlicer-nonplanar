# ADR-0006 — bounded primitive clearance queries

Status: implemented and natively tested for A04; independent A08 review pending.
Owner: current implementation. Extends ADR-0005;
the IR and the immutable normative package are unchanged.

Tool-local axes remain parallel to physical machine axes. There is no automatic
rotation with the tangent. The supported matrix is annular horizontal tip vs
solid half-space below an affine plane, and translating axis-aligned tool box
vs static physical box. Cross pairs and deposition-contact exceptions return
UNKNOWN. The tip stores opening and outer radii separately; the plane extremum
uses the outer radius. A tip alone makes no claim about a complete nozzle/head.

For the plane pair, signed normal clearance is
`(z - a*x - b*y - c - r*sqrt(a*a+b*b))/sqrt(1+a*a+b*b)`.
The centre includes the tool-local offset. Its minimum on a straight segment
is at an endpoint, even for constant Z and transverse slope. The inner opening
does not change this extremum on an annulus.

For boxes, each axis gap is the maximum of the two directed face separations.
Outside the boxes, clearance is the Euclidean norm of positive axis gaps.
Inside, it is the largest (negative) axis gap, i.e. the negative least face
translation needed to separate the boxes. This continuous signed function is
evaluated with outward intervals over a whole parameter interval. Subdivision
tightens the bound; samples only supply upper bounds/witnesses, never PASS.
This avoids accepting the bounding-box overlap itself as a collision and also
handles a diagonal path whose swept bounding box overlaps a remote obstacle.

Each binary operation and square root is rounded outwards with nextafter in
IEEE binary64, round-to-nearest with gradual underflow, without
reassociation/contraction/fast math. Unsupported floating environment returns
UNKNOWN. Input position coordinates and tip radius are bounded by the A03
10000 mm domain; nonfinite/overflowed plane arithmetic is UNKNOWN. Existing Orca line/box
helpers return point estimates without a certified arithmetic/sweep bound, so
they cannot supply this contract. No additional geometry dependency is needed.

Both distance endpoints include arithmetic bounds. Separately charged input
errors are the full NumericBudget plus tool measurement, positioning and
material-envelope uncertainty. These errors are symmetric bounds on the
signed clearance; callers must bound the combined relative uncertainty, not
one object's error when both move. PASS needs lower > required; FAIL needs a
witness upper < required. Equality/uncertainty never passes. A FAIL at positive
required clearance can mean insufficient clearance without actual penetration.

Resource limits apply to interval evaluations and a steady-clock deadline;
no partial PASS survives exhaustion. Result bounds, when present, cover the
global minimum; witness time is not asserted to be the first time of contact.
Reports preserve component/obstacle/event identity and the checked [0,1] domain.

This is internal geometry contract version 1. No persistent profile, project,
cache or verification report consumer exists to migrate. Future serialization
must bind this version, primitive definitions, budgets and capability matrix.
Scene coverage, moving links, cones/meshes/spheres, sequential material and
calibrated contact remain A07/B05/B08 work; byte replay remains A06/B12.
