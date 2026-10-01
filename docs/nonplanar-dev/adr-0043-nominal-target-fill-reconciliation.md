# ADR-0043: nominal target deficit and vertical spill

Status: bounded geometric primitive implemented; complete B07 planner and
independent safety review pending.

ADR-0042 measures finite occupied union U independently of commanded sum S and
multiplicity excess R=S-U. Equal U and target volume V do not prove target fill:
material may occupy the wrong part of the solid. Add protected internal
material-fill contract 1 within DepositionModel. Existing material/IR
serialization, projects, profiles, fixtures and export contracts are unchanged.

The target T is the complete solid between the actual nominal laid-body roof
and its owned affine first-pass surface over an exact rectangular XY domain.
The input protected integral proof owns this roof source, cell and V interval;
caller-edited wrapper status, amounts or reasons cannot replace that proof.
The protected union snapshot owns the candidate prefix and physical XYZ box.
That box must have exactly the target XY domain and contain all of T vertically.
The result concerns material inside this box. Material outside its XY boundary
remains a separate obligation for perimeter/seam and full-job integration.

Measure B=|D_nominal below the actual roof| and A=|D_nominal above the affine
surface| within the box. The source first-pass proof already establishes a
strict positive separation of roof and surface, so these sets are disjoint.
Publish separate nonnegative outward intervals for outside O=B+A, covered
C=U-O, and missing M=V-C. Intersect with known nonnegative/subset bounds and
require every published interval, including V and the captured U, to meet the
one requested whole-domain mm3 width. Missing and outside cannot cancel each
other. R remains a separate measure in the retained union proof.

Reuse the finite union integrator privately with three clipping policies.
Only the ordinary box policy can construct a public MaterialUnionSnapshot.
The two special windows return private amounts, so a roof-clipped result
cannot masquerade as a certified ordinary-box union. For B use physical Z
and the actual nominal roof enclosure. Refine the protected body prefix with
the same continuous whole-cell nominal roof helper as the target integrator;
only wholly covering footprints raise its lower roof bound. The minimum of
all retained proof-cell lower roofs is a certified global floor and changes
no actual roof. Inherited body candidates discard irrelevant geometry.
This avoids repeatedly querying broad AABBs of fine diagonal proof leaves.

For A use the unit-Jacobian shear q=z-gx*x-gy*y with the target affine gradient;
the target q is constant. Original physical Z planes still clip the domain.
The concavity shortcut for ordinary-box overlap is disabled for special
windows: a body roof must not be assumed concave. General inner/outer profile
and multiplicity bounds remain valid. Special-window refinement requires
precision of U only; auxiliary S/R bounds never become public certificates.

Cache the correlated affine floor coefficients Z_start-h_start and
DeltaZ-Delta h before evaluating t. This avoids artificial longitudinal
uncertainty when top and gap slope together. Below-roof refinement alternates
actual body-roof and candidate directions; midpoint probes only choose a
candidate split direction. They never provide volume, coverage or acceptance
bounds. Every probe and actual roof/section evaluation is charged to the same
budget. Exact clipping of a fractional current axis-aligned butt removes the
conservative binary64 AABB remainder before any individual lower bound.
Future records remain absent.

Both sources must have the same revision and model ID. Their declared geometry
origin must match, or the candidate ledger must name the exact complete body
ledger hash as parent. Parent verification rejects an incomplete body prefix.
The native body adapter already uses this latter derivation. This binding
establishes declared source association; full software/job/plate/tool/scene
provenance is still B13 work. Source handles and limits are captured before
callbacks, and both revisions are checked again before publication.

Defaults remain 0.001 mm3, 4095 cells, depth 32, 200000 work units and a
one-second cooperative deadline. Hard ceilings remain 65535 cells, depth 32,
2000000 work units, 200000 source records and 30 seconds. Parent hashing,
proof-cell preparation and both clipped integrations share the same work/cell
budget and deadline. The protected input proofs have their own already-recorded
construction costs. Reserve half the remaining error width for outward final
arithmetic; each clipped component receives at most a quarter. Coarse inputs,
invalid domain/source, exhausted budgets, cancellation, stale revision,
nonfinite or unsupported arithmetic publish no fill snapshot. Provisional
components on UNKNOWN remain diagnostics only. No fill cache is persisted;
future cache keys must include this contract, software, both sources/prefixes,
domain, precision/resource policies and all job dependencies.

Five analytical cases distinguish correct fill, current fraction/future
exclusion, below-roof spill, equal-total misplaced fill, transverse affine
slope and a rounded actual roof against independent triangle/circular
integrals. Native evidence measures all first-line candidate packets against
the exact 1:16 wedge's actual body. These are declared geometric measures,
without physical bead/contact, accepted path order or motion/flow qualification.
Underfill localization and a planner that resolves it, perimeter/seam, later
actual support, curved paths and independent final-byte replay/export remain
required. The public guarded-hybrid blocker stays closed.
