# ADR-0018: connected slope masks with explicit holes

Status: implemented and native tests pass; independent review pending.

Continue the nominal B03 geometry contract by partitioning slope-selected
upward facets along shared mesh edges. Preserve exact owned mesh face/vertex
IDs. Each patch must have a simple outer projected boundary and zero or more
simple hole boundaries, oriented by the upward triangles. Directed boundaries
must have exactly one incoming and one outgoing edge per vertex; pinches,
crossings, overlapping edges or nonadjacent boundary contacts return UNKNOWN.
Do not fill a hole, merge a vertex-only contact, silently discard a component
or change the source geometry to obtain a valid mask.

A boundary's signed XY area uses outward intervals. One positive outer loop
and negative hole loops are required per edge-connected patch. Retain the
triangle-area bound and verify consistency with the signed boundary sum.
These are nominal topology/area checks; they add no footprint inset, curvature,
import-error envelope, head access, transition or export qualification.
Cancellation/deadline/staleness still cover the whole analysis, including all
boundary work, and no partial patch collection is published.

Predefined independent fixtures: a 3 x 3 mm plate with a 1 x 1 through-hole has
one patch, outer signed area +9, hole signed area -1, total 8 mm2. A 3 x 3
heightfield with flat side strips and a steep middle strip has two 3 mm2 patches
at slope limit 0.1. A 4 x 4 heightfield with raised vertices at (0,0) and (2,2)
produces a slope-mask hole touching an excluded corner at (1,1); the source is a
valid solid, but this pinched mask must be rejected. Whole-projection and
material/shape preservation tests remain mandatory.
