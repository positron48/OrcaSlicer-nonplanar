# ADR-0039: finite hatch cell ownership and complete volume remainder

Status: bounded volume partition implemented; finite beads, full B07 and review pending.

ADR-0038 attaches whole-parent strip volumes to finite centerlines. Those strips
extend beyond the path ends and outermost widths. Charging them all to a bead
would place boundary material in its E without a matching finite footprint.
Partition each pass into finite rectangular path-owned cells and a complete
explicit end/boundary remainder before selecting a deposited bead amount.
Allowed changes are DepositionModel, geometric/native consumers and records.

Internal affine-hatch contract becomes 2: only its validated factory constructs
a hatch snapshot. Immutable copying remains possible; unrelated aggregate input
cannot supply invented endpoints or cells. New hatch-cell contract is 1, also
with a factory-only snapshot. Material-integral contract stays 2; its stored
continuous proof and meaning are unchanged. No persisted hatch/cell cache exists.
IR, body/material fingerprints, profiles, 3MF and export formats are unchanged.
Future dependency keys include the complete source, revision, geometry, policies,
numerical limits and these internal versions.

The existing material model has finite flat longitudinal ends and rounded or
rectangular transverse sections. This stage partitions its XY enclosing
rectangles; it does not introduce hemispherical longitudinal ends. Use stored
path end coordinates along the hatch direction, and inward-rounded half-width
limits across the first/last rows, to define an interior rectangle. Intersect
each previous strip with it. Verify every finite cell lies wholly inside that
line's nominal flat-ended rectangle using exact binary64 rationals. Verify
adjacent cell edges coincide and span the entire interior, then surround it by
four disjoint rectangles covering the complete parent remainder. Exact summed
area must equal parent area. Closed common boundaries carry no extra volume.

Reuse the protected continuous first-pass nominal-roof proof. Its private
rectangle integrator clips every exact leaf against all four rectangle bounds,
reuses the valid roof interval on the subset, evaluates exact affine moments and
checks exact complete area. The old strip API calls the same implementation;
its original limits/reasons and native analytic enclosure remain tested. No
fresh material query or sampled acceptance is introduced. Later prospective
cells integrate between stored affine pass surfaces, with no assumption that
those previous cap paths have already been deposited.

Sum finite and remainder lower/upper endpoints separately in eager exact
arithmetic. Check each pass against its parent volume and the whole partition
against the stack's total precision. Rounding cannot hide lost or duplicated
end volume. Capture source/limits before callbacks. All first-proof fragments
and later affine cells share the caller's cell/evaluation budgets and one
cooperative deadline, with hard maxima 65535/200000/30 seconds. No result is
published after staleness, cancellation, deadline, work, area or precision failure.
Hard worker memory containment and full-cycle performance remain pending.

These volumes are target cells, not finite rounded-section material unions or
E. Transverse voids, overlap, actual local gaps, adaptive constant-flux deposition,
perimeter/seam construction, later actual support and legal order remain required.
The complete explicit remainder prevents an allocator from silently borrowing
end/seam volume for a shorter line; it does not itself build the missing seam.
Full-head/contact CCD, independent final-byte replay and unified export remain
pending. The existing safe-hybrid slice/export blocker remains in force.
