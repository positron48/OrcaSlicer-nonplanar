# ADR-0020: exact affine curvature domain and crease exclusion

Status: implemented and native tests pass; independent review pending.

A triangulated nominal heightfield is piecewise affine. A noncoplanar internal
edge is a crease, where a finite classical smooth-curvature bound is unavailable.
Do not estimate a finite bound by averaging normals or dividing a normal change
by an arbitrary mesh spacing. Record every such selected internal edge using
exact coplanarity predicates; coplanar triangulation edges are not creases.

Each patch records its height range, maximum slope upper bound and a curvature
upper bound of exactly zero only when all its facets are coplanar. Otherwise
the patch curvature remains unknown. A separate affine-footprint query first
requires whole-capsule XY containment, then strict clearance from every crease.
A connected capsule avoiding all creases lies on a single affine sheet, so its
nominal curvature bound is zero even within a larger nonaffine patch. Reaching
a crease returns UNKNOWN, preserving the distinction from outside-mask geometry.
The query shares one cancellation/revision/deadline boundary with containment.

Predefined oracles: the block and affine wedge remain accepted across arbitrary
coplanar diagonals. A 3 x 3 grid with plateau z=2 for x<=1, ramp z=1+x for 1<=x<=2,
and plateau z=3 for x>=2 has six unit crease edges (x=1 and x=2). Radius-1/8
footprints running along y within each strip are affine; a cross-strip footprint
has admissible endpoints but unqualified curvature. A radius-1/4 disk at x=3/4
exactly reaches the first crease and must reject. Replacing the rise by one
binary32 ULP still creates six creases: no epsilon may erase them.

This extends only the nominal affine domain. Smooth reconstruction and bounded
curvature of curved STL/STEP surfaces, imported geometric uncertainty, actual
contact/head/scene access, motion limits and export remain separate obligations.
