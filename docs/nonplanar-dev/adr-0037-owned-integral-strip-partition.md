# ADR-0037: owned continuous integral proof and exact strip allocation

Status: bounded allocation primitive implemented; complete paths/B06 and review pending.

Finite paths need local volumes, not only ADR-0036's whole-cell quotas. Re-running
all material queries for every stripe would repeat roof/coverage work and could
produce different stopping partitions. Retain the already-proven continuous
nominal roof partition from ADR-0035 and reuse it for exact strip integration.

Internal material-integral contract becomes 2. Bounded results own an opaque
immutable proof containing the prefix, affine target and all exact closed leaf
polygons with nominal roof intervals. Only the integrator constructs it. Keep
depth-terminal leaves even when removed from the refinement heap; unknown/error
results carry no proof. Parent diagnostic fields do not authorize altered
geometry or volume. Existing IR, body/ledger fingerprints, profiles, 3MF and
export formats remain unchanged. No persisted integral cache currently exists.
Future dependency/cache keys must include contract 2 and the complete parent
source, geometry, numerical/precision policies and revision.

A strip request supplies X or Y and strictly increasing binary64 cuts, including
both exact parent boundaries. Capture the proof, limits and bounded cuts before
callbacks. Clip every owned leaf to each strip with eager exact half-planes.
Reuse its roof bounds over that exact subset and compute affine cap moments and
area exactly before outward conversion. Zero-area closed contacts carry no
volume. Exact accumulated fragment area must equal the complete strip area;
missing boundaries, duplicate/reversed cuts, omitted regions and unsupported
axes cannot publish an allocation.

Sum lower/upper volume endpoints separately in exact arithmetic, then convert
outward for each strip and the whole partition. Require positive strip volume,
consistency with the parent interval and the explicit whole-partition precision.
The bounds may improve slightly through tighter exact fragment moments; they
still enclose the continuous same nominal integral. No fresh bead query or sampled
acceptance is involved. Strip bounds are prospective cell volumes, not selected
beads, E or evidence that a fixed-width path fills the cells.

Limit cuts to 4097, fragment cells to 65535 and leaf/strip evaluations to 200000,
with the same maximum 30-second cooperative deadline convention. Native evidence
requests 32768 fragments / 200000 evaluations / 5 seconds / 0.01 mm3 precision.
The 8191-fragment refusal remains a negative; actual X/Y allocation produces
17620/17614 nonzero fragments, with 27252 evaluations in each case. This is a
larger explicit work allocation, not a weaker geometry/volume tolerance. Hard
worker memory/time containment and full-cycle performance remain pending.

Allowed scope: DepositionModel APIs, their geometric/native consumers and
execution records. Independent circular/affine integrals and the existing
Clipper/monotone-Z native oracle remain required. The actual source-bound first
surface now consumes the strip API in native tests. Next derive finite path
geometry, nominal bead volume and seam corrections from these intervals,
preserving fixed width and actual later support. Full-head/contact checks, legal
motion ordering, independent final-byte replay and guarded export remain open.
