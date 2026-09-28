# B03 exact affine curvature domain

The nominal mesh now distinguishes actual noncoplanar creases from coplanar
triangulation edges with exact predicates. Each selected patch retains crease
edge/face ownership, height range and maximum slope upper bound. Curvature is
zero for an exactly affine patch and otherwise unknown. ADR-0020 explains why
averaged normals cannot establish a finite smooth-curvature bound at a crease.

The affine-footprint query shares the whole XY capsule containment path and
its deadline/revision checks, then checks the full capsule against every crease.
Avoiding all creases keeps the connected footprint on one affine sheet. Reaching
a crease returns UNKNOWN with no curvature bound. Source geometry is preserved.

Three predefined tests fail against the previous empty metadata/stub query
(exit 42). The implementation passes block/wedge and subdivided-plane cases;
the piecewise plateau/ramp has exactly the six predefined unit crease edges.
Each strip admits footprints, while a cross-strip segment and exact crease
tangency remain unqualified. A one-ULP rise still retains all six creases.
Late cancellation, staleness and deadline never retain a curvature bound.

macOS ARM64 Release application/native targets build with exit 0. Combined B03
selection passes 19 cases/420 assertions with NoAssertions. Selected CTest runs
177/177 without skips. Six fresh OFF/ZAA comparisons pass under the established
timestamp/additive-OFF-setting allowances. Commands, exit codes, red/final XML,
CTest discovery, baselines and source/binary hashes are in evidence/B03-curvature.
Author review covered exact crease classification, transitive coplanarity and
shared final publication checks; independent review remains pending.

This qualifies only nominal affine curvature. Curved surface reconstruction,
import uncertainty, actual tool/scene access, area-clipped ROI, body/cap planning,
job ownership, guarded export, Linux confirmation and physical tests remain open.
