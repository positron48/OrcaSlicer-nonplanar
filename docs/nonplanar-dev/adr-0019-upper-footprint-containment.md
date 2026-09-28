# ADR-0019: whole-segment nominal XY footprint containment

Status: implemented and native tests pass; independent review pending.

The next B03 invariant is that the entire closed disk swept along a candidate
XY segment fits strictly inside one selected nominal patch. Endpoint membership
alone is insufficient: the segment may cross a hole or its footprint may reach
an outer edge. Represent the inset implicitly by this containment query, retaining
the source mask and holes rather than producing a rounded polygon offset.

Use an explicit positive XY footprint radius, nonnegative boundary uncertainty
and transition inset. Sum these with an outward upper bound, keeping zero terms
exact. Require the first center to be inside the outer loop and outside every
hole, and the entire segment to have exact squared distance greater than the
squared radius bound from every boundary edge. Equality is rejected. This proves
nominal XY containment only; it does not establish surface curvature, Z contact,
physical bead shape, actual head/scene clearance, or physical qualification.

Snapshots are constructed only by the audited analyzer. Query, limits and owned
snapshot are captured before callbacks; cancellation, deadline, staleness or
unsupported arithmetic discard the decision. No production/export route opens.

Predefined analytic oracles: a radius-1 capsule from (2,2) to (18,8) fits inside
the 20 x 10 block and affine wedge. A 3 x 3 ring with central [1,2] x [1,2] hole
accepts a radius-1/4 capsule along y=1/2, but rejects a segment from (1/2,3/2) to
(5/2,3/2) even though both endpoints separately fit. A stationary disk with center
(1/2,1/2) and radius 1/2 touches the boundary and must reject; the immediately
smaller binary64 radius passes and the immediately larger one rejects. Growing
any margin cannot turn rejection into acceptance. Tests also bind captured inputs
and late revision/deadline checks; no sampling-based proof is accepted.
