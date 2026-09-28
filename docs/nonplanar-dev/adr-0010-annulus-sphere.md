# ADR-0010: finite annulus versus solid sphere

Internal geometry contract version 2 appends SphereObstacle to the scene variant.
No persistent consumer exists; plane/box variant alternatives retain their order.
The only new supported pair is rigid finite horizontal annulus / solid sphere.
Box/sphere, contact exceptions and other unsupported pairs remain UNKNOWN.

For centre separation (dx,dy,dz), radial distance rho=sqrt(dx^2+dy^2) and annulus
radii ri,ro, nearest radial gap is max(ri-rho,0,rho-ro). Signed sphere separation
is sqrt(radial_gap^2+dz^2)-R. A negative value means the solid sphere intersects
the annulus. The opening is empty; replacing the annulus with its outer disk
would incorrectly reject a sufficiently small sphere inside that opening.

The existing outward interval arithmetic bounds the complete moving centre
trajectory on each parameter interval. The existing bounded priority queue,
uncertainty expansion, strict inequalities, deadline and work limits are reused.
This is neither a sampled-only PASS nor a full-head spherical collision solver.

Independent examples fix expected rim/hole distances and the 3-4-5 triangle,
axial sphere distances at translated coordinates, and an interior collision with
free endpoints. Tests were red while the new variant was unsupported, then pass.
Native consumers remain tests; ProfileScene uses explicit SceneBox inventories
and has no variant loader to migrate. Existing plane/box negative cases remain.
