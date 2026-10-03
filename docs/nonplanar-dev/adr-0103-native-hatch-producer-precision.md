# ADR-0103: native hatch producer precision

Date: 2026-10-03. Status: bounded implementation; independent safety review pending.
Parent: cf15cab28c7b2de78b5703246fb4c4f48eb35a8a. Follows ADR-0102.

The combined native hatch planner built its first-pass roof integral at the
stand-alone parent precision. A later strip partition reuses immutable owned
roof leaves and cannot refine that certificate. A wider actual native ROI over
rounded shoulders therefore refused INTEGRAL_STRIP_GLOBAL_PRECISION even when
the original integral resources could support the selected consumer precision.

Before integrating the parent, use the minimum of the original material interval
width and half of the selected hatch volume interval width. The remainder is
reserved for clipping/outward strip sums. This is a tighter producer requirement,
not permission to exceed the original consumer budget. The existing strip and
whole-hatch checks remain authoritative and can still refuse. A tighter explicit
parent limit is preserved. No retry, new work/cell ceiling, numerical margin,
contact exception or exported permission is introduced.

The native planner already copies body/request/policy/limits before callbacks;
the selected precision belongs to those copies. Existing parent work, cells,
depth, cancellation, current revision and nested deadlines remain. Any insufficient
resource or irreducible uncertainty publishes no native hatch snapshot. A valid
coarse stand-alone result remains valid only for its original width; attempting
its finer split still refuses. No private/public serialization version changes.

Two wider native fixtures retain the complete actual Orca body and exercise
parallel/transverse strip partitions across three existing stadium rows. Their
synthetic 1 mm body line setting is inherited from the native fixture, not U1
calibration. Both selected integrals fit .0005 mm3 and strips fit .001 mm3 under
the original 8191 cells/200000 evaluations. Complete first-cap construction still
refuses original FIRST_HATCH_ROOF_DEPTH_LIMIT/FIRST_HATCH_ROOF_SEGMENT_LIMIT.

An independent checker decodes/hash-checks every actual journal row, derives
constant stadium widths from actual doses and rational pi/root bounds, and
integrates the maximum roof over a complete closed-slab partition. It includes
shoulders/overlaps and finite butts, proves selected rows cover above the plane
before ignoring older rows, and verifies every independent integral enclosure
lies inside the produced parent/strip intervals. Unsupported geometry refuses.
It checks volume for the supplied selected surface, not source-to-plan geometry,
Lower support, cap dose, contact, whole-job or physical qualification.

Original candidate bytes and journals are retained. Fixed17 stays4 PASS/RUN and
13 UNKNOWN/NOT_RUN, full B01-B15 IN_PROGRESS and export BLOCK. Next implement a
certified affine roof chord where the constant roof approximation exhausts the
original packet/roof budget, then qualify a positive native density recipe and
its captured controller/child/final-byte ownership. Full fill/seams/layers/head/
contact/routes/order/source/software/physical/publication remain separate.
