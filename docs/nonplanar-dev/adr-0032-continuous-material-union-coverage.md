# ADR-0032: continuous material-union footprint coverage

Status: implemented bounded query; independent review pending.

A prefix and a physical axis-aligned XY footprint x vertical range define one
continuous query. Return Covered, Uncovered with a certified actual point witness,
or Unknown. Bind the owned prefix, original domain and nominal/upper/lower tag in
the result. A zero-thickness plane is valid; invalid or exhausted contexts cannot
be Covered. This is a geometric statement in declared material, not export proof.

Project a convex footprint cell into each straight bead's longitudinal/transverse
coordinates. These projections are affine: their vertex extrema enclose every
point in the convex cell. Feed the resulting intervals and the complete vertical
range into the actual nonlinear width/gap/stadium membership calculation. Inside
certifies the entire cell; Outside excludes the entire cell; Unknown refines.
This is not vertex membership or point-grid coverage. Varying-gap lofts need not
be convex; their nonlinear section calculations retain outward bounds.

Use eager exact GMP rational polygons. A half-plane split produces closed children
whose union is exactly the parent, so no rounded slit disappears. Align proof
cells to relevant long beads for diagonal infill efficiency; this never reorders
motion/material. Cache only candidates not certified Outside in a parent. Child
cells are exact subsets, so excluded parent material remains excluded. Bound
records, evaluations, cells, depth and vertices; cooperative checks include final
publication. Opaque rational work and hard memory containment remain open.

A cell center may prove Uncovered only after its rounded actual coordinates are
certified within the exact cell and Outside every still-relevant bead. Sampling
never proves Covered. Unresolved depth/boundary cells remain Unknown while other
cells may still supply a counterexample. All cells must be certified Inside some
bead before returning Covered. A hole between material-covered corners is negative.

Reuse the same interval section primitive for point and whole-cell query, without
new volume/width interpretation or an IR/ledger serialization migration. Clamp
projected parameters to the present [0,current_fraction] support domain before
section bounds; longitudinal containment is still independently required. No
phantom zero-prefix bead or future geometry enters the candidates.

Native evidence proves the complete 6x6 mm interior footprint at Z=1.9 inside the
actual lower reserved-body material; Z=2.5 is uncovered. An independent long-double
section construction and integer Clipper polygon subtraction confirm the positive
with deliberately shrunken, quantization-safe rectangles. The native domain is
flat, constant-gap body segments, not all curved/stepped/edge/seam geometries.

SIGBUS during initial exact arithmetic was an installed GMP Apple ARM64 ABI
violation, not a coverage permission. Both failures are retained. Eager rationals
alone did not cure it; ADR-0033 supplies the actual pinned dependency correction.
The corrected native integration/100 randomized repetitions/OFF/ZAA evidence is
shared by reference. Contact, complete head CCD, cap volume/seam and actual job
worker/planner binding remain mandatory unfinished work.
