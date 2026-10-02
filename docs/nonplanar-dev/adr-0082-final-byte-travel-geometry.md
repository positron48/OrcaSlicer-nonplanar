# ADR-0082: independent continuous geometry of a final-byte Travel block

Status: bounded B12 component implemented; full B01–B15 remains active.
Scope: independent verifier, diagnostic CLI/JSON, analytical/native tests and
Linux workflow. Normative files, planner math, stock writer/profiles and
production project formats are unchanged. Independent safety review is pending.

The independent rate/material reconstruction already owns exact final decimal
XYZ/E and declared largest/smallest dose sections. Add geometry verification of
one maximal contiguous non-depositing Travel block against all actual material
before that block. Reject skipped first/last adjacent Travel rows, mixed roles,
out-of-range indices and incomplete scenes. Later deposits never participate.
This component does not certify other motions or the complete job.

Use a separate version-1 synthetic scene/query: complete fixed-axis flat annulus,
all six declared head roles with unique IDs and outer boxes, all-configurations
enclosure of moving parts, static obstacles, nozzle/scene coverage, omitted-part
height and uncertainty. Retain all nine named clearance terms from the original
scene policy. Inflate complete swept world boxes by their exact rational sum
plus scene uncertainty; the cube conservatively encloses the required Euclidean
clearance. Do not infer measured U1 geometry or physical delivered-material bounds.

Partition progress × local XYZ for boxes and progress × local XY for the flat
annulus. Exact radial inequalities can remove a complete tile wholly inside the
opening or outside the outer disk. Every remaining accepted tile encloses its
whole affine sweep and excludes all declared static and previous Upper solids.
Outward Upper BVH node/individual bead boxes prune strict disjointness only;
retained intersections still require independent exact rational section tests.
The opening is not a filled disk. A rigorously enclosed actual tool point inside
Upper/static can establish FAIL; sampled clearance never establishes PASS.
An unresolved required margin or any exhausted budget returns UNKNOWN.

Private immutable proof owners retain the exact material source, complete
pre-block prefix, copied scene and all complete partition leaves. Original
work/cell/depth/one-second deadlines apply; nested prefix work shares the root
deadline. Copy all caller inputs/options before callbacks. Cancellation, stale
source/rate/material/scene, unsupported rounding/underflow and callback exceptions
discard partial proofs. Recheck before/after publication, including negative
witnesses; late cancellation cannot preserve either PASS or FAIL evidence.

Extend the existing headless executable with
`--linear-travel-geometry-only RATE.json MATERIAL.json QUERY.json CANDIDATE.txt`.
Bound file sizes and enforce exact JSON registries, scalar types, finite numbers,
array dimensions, duplicate keys and depth. Reconstruct current rate/material
inputs before geometry under one absolute one-second root deadline. Exit codes
0/2/3 mean component PASS/FAIL/UNKNOWN; job status is always UNKNOWN and export
false. This diagnostic schema is not a persisted/authenticated job certificate.
Keep the fixed 17-check job registry and all thirteen missing full-job domains.

Separate 113-bit equations replay final text and check every accepted swept cell,
every previous largest-dose bead, static exclusions, complete local/time domain,
partition measure and pairwise interior disjointness. They call no planner or
verifier geometry/BVH. Restricted finite reference domains and explicit numerical
comparison guards refuse unsupported reference cases. The native test uses the
actual owned B13 .28 mm departure candidate, original six head roles, margins,
material errors and three complete Travel legs; no planner clearance is reused.

Retain original wide-exit FAIL and narrow rounded-section UNKNOWN. Full deposition
contact, fixed-width cap fill/seams, earlier/prolog/parking/end/order, physical
delivery/head qualification, firmware transforms, source/software/resources and
all-route atomic job publication remain required. Synthetic scenes stay
unconfirmed and guarded export BLOCK. Standard U1 head/.4 nozzle is known;
firmware version and material brand do not gate this software implementation.
