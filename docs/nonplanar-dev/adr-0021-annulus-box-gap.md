# ADR-0021: finite annulus versus a solid axis-aligned box

Status: implemented and native tests pass; independent review pending.

SimulationScene inventories contain boxes, but the rigid annular tip/box pair
is currently unsupported. Geometry contract version 3 adds this pair and an
explicit result metric: existing pairs retain SignedSeparation; this pair uses
UnsignedGap. A zero gap does not claim a penetration depth. At zero required
clearance it remains UNKNOWN; at positive required clearance a bounded witness
can establish insufficient clearance. Strict PASS, uncertainty expansion and
the continuous adaptive interval algorithm remain unchanged.

For a fixed annulus center, the XY rectangle's attainable radial distances form
the continuous interval [rho_min,rho_max]. Its gap from annular radii [ri,ro] is
max(0,rho_min-ro,ri-rho_max). rho_min is the norm of the nearest coordinate gaps;
rho_max is the norm of the farthest coordinate magnitudes. The vertical gap is
the distance from the annulus plane to the box's Z interval. The Euclidean norm
of these two independent gaps is the exact nonnegative set distance. Evaluate
that expression with outward intervals for the whole moving-center interval;
sampled values supply witnesses/upper bounds only.

Predefined independent oracles: radius-1 outer rim, rectangle starting at XY
(3,4), lower Z=3 gives gap sqrt((5-1)^2+3^2)=5. A centered rectangle of XY half
sizes 3/8 and 1/2 lies within radius 5/8; an annulus with inner radius 1 has gap
3/8 through its opening. Translating that annulus from x=-4 to x=4 across a tiny
central box has clear endpoints AND midpoint, but must detect intermediate rim
contact. Required zero and positive clearances have distinct expected outcomes.
Translation, reversal and larger outer radii must preserve their geometric rules.

No persistent consumer uses the internal geometry version yet. The prior
unsupported-pair test is retained with the still-unsupported box/sphere pair;
the newly supported annulus/box pair receives independent positive and negative
tests. Deposition exceptions, arbitrary meshes, profile qualification, actual
printed material and export remain outside this contract.
