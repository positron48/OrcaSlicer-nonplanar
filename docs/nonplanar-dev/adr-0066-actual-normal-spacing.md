# ADR-0066: actual normal spacing for a later affine footprint

Status: bounded software contract implemented; independent review and physical
qualification remain pending. Scope: DepositionModel and both native test consumers.
Original normative documents, geometry, profiles and guarded export policy remain.

## Invariant and definition

For each point of the original assessed later affine rectangle, follow its
unit normal downward until the first intersection with the actual previous
D_nominal union. That distance must lie inside the original stack's later normal
minimum/maximum. This is a geometric normal distance to actual material, including
rounded shoulders and finite ends; it is not an offset to an infinite prospective
plane. Normal direction does not change the fixed orientation of the tool.

Every accepted XY leaf encloses all rays from distance zero through the original
minimum in an empty XYZ box, and encloses all endpoints at the original maximum
in a completely occupied nominal run/event union box. The two whole-domain
certificates bound the first intersection of every ray. Binary XY partitions
cover the complete original parent with disjoint interiors. Near-bound equality
may conservatively refuse. Terminal occupancy can refuse a valid thin/material
configuration when the maximum endpoint is already below it; it never infers
occupancy from a bounding box or changes the physical normal limits.

Only original completed rows and the exact actual current fraction participate.
Nominal sections remain distinct from Upper collision envelopes and Lower support
certificates. An actual nominal section cannot exceed its affine axis top; that
bound only excludes impossible near candidates. Complete point-ray boxes may
refute universal coverage, never accept a footprint. No ray may leave the original
final-surface XY ROI; no support is extrapolated outside it.

## Errors, resources and ownership

Original corner height errors enclose both the affine height and normal direction.
Source/hatch and actual material coordinate errors inflate all ray coordinates.
The spatial error includes three affine corner weights, all three coordinate
uncertainties and maximum-distance normal-direction displacement; their combined
bound must fit 0.05 mm. This proof's distance error stays in its own snapshot.
It does not become new deposited-coordinate error: the existing bead dose/path
solver retains its original numerical field and downstream material losses.

Capture the input snapshot and limits before any callback. Charge source/run
walks, every candidate classification, failed and successful terminal queries,
outer partitions and diagnostic refutations. Terminal trials use one inner depth;
outer XY adaptation shares the original total depth. Carry cells/evaluations into
the existing next-bead solver and its roof queries. Both original producer
absolute deadlines, stale/cancellation/rounding callbacks and final publication
checks remain active. Resource exhaustion publishes no partial snapshot.

## Integration and versions

First-cap-normal-spacing runtime contract is version 1. Next-cap-bead runtime
contract advances from 2 to 3 and requires its owned normal certificate in the
private constructor. plan_next_cap_bead must obtain that certificate for the
whole supported parent before deriving any later packet. Both normal and dose
stages use the same original producer work/cell/time limits. The first-bead
solver starts with zero work as before; later solving starts with already spent
normal work. No margin, timeout, precision, amount or loss is enlarged.

No persisted proof/cache, scene, ledger, motion IR, 3MF, profile schema or source
fingerprint formula changes. The ordered later material factory still consumes
only protected bead snapshots. Complete job/replay/export integration is pending.

## Verification and limitations

Independent 113-bit plane distances, exact leaf area, maximum actual axis tops
and whole nominal run section boxes check flat/sloped X/Y positives. An original
0.20-0.21 mm normal band above a sole actual rounded contour fails although its
vertical/support assessment succeeds; an independent circular section roof shows
first material farther than 0.21 mm. Future contours/infill cannot repair it.
A tiny source cell with admitted corner error refuses the derived spatial budget.
Original resource, caller-mutation, stale, cancellation, late-publication and
rounding negatives remain. The native 2034-body-row / 173-cap-packet fixture and
its actual eight-packet later bead retain their original geometry and limits.

This is local affine nominal geometry. General curved/stepped interface, complete
shoulder/seam/fill/excess, measured contact/head/flow/order/entry/exit/travel and
whole job/software/resource/final-byte replay remain. Physical tests stay NOT_RUN;
public guarded export stays BLOCK; full B01-B15 stays IN_PROGRESS.
