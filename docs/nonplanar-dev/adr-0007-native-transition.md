# ADR-0007 — derived body before Print and bounded first-pass cells

Status: ACCEPTED_FOR_A05_EXPERIMENT; independent A08 review remains pending.
Extends the internal geometry contract without changing MotionEvent v1 or any
persistent profile/cache/3MF format. One contract owner in this task.

## Candidate body hook

Use a job-owned derived body mesh before `Print::apply`, preserving the source
model. For the horizontal reservation spike, reuse `cut_mesh` with capped
upper/lower results. Native `Print::process` then calls make_perimeters/slice,
infill and top-shell generation on the lower body only. This is preferable to
editing generated fill surfaces: make_perimeters restores untyped surfaces and
subsequent shell generation could reconstruct the reserved cap.

The native test establishes this hook only for a closed analytical wedge, a
horizontal cut and identity geometry transforms. `cut_mesh` takes float Z and
is not a certified arbitrary-mesh boolean; B04 still needs topology, holes,
multiple patches, placement, invalidation and volume/error qualification.

## Actual support, not CAD

Reconstruct a support core from an actual native ExtrusionPath segment, layer
print_z, captured NativeScale, explicit object-to-physical translation, nozzle
diameter and source ID. Only noncontoured axis-aligned solid-infill segments
with ordinary rounded-rectangle Flow area are supported. Reject sloped/scarf,
bridge, sparse, nonzero native Z and adjusted-volume paths in this spike.

For width w and height h, the rounded-rectangle model has a flat central width
w-h. This smaller core is used for lower support, never the full expanded bead
width. Erode endpoints and lateral bounds by the declared XY uncertainty and
coordinate-conversion bound. Preserve nominal top separately from lower/upper
top elevations, which include the specified vertical material uncertainty.
Physical validity of that bead model requires later operator qualification.

## First-pass cell

An exact physical XY rectangle and three corner Z values define an affine cap
cell; the fourth is derived. Corner errors propagate through that relation.
Every corner gap must lie strictly inside [minimum,maximum] after arithmetic,
surface and support errors. Definite too-small/too-large gap is rejected;
boundary overlap, missing lower coverage or unsupported geometry is UNKNOWN.
The full affine footprint is checked, including transverse slope.

Nominal deposited volume is area times mean affine vertical gap. A separate
volume interval includes corner/support uncertainties. No 3D-length correction,
implicit Z lift or substitution of D_upper for D_lower occurs. Compatibility
here describes this cell only, not nozzle/head clearance or export permission.

Shared outward interval arithmetic retains A04's binary64/nearest/gradual-
underflow preconditions and source-specific strict compiler flags. No new
native scale or serialization convention is introduced. Later persistent
consumers must bind this internal transition contract and support provenance.
