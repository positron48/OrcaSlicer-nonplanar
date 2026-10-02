# ADR-0084: declared recent forming-run contact at the finite working face

Status: bounded B12 component implemented; full B01–B15 remains in progress.
Scope: independent final-byte verifier, diagnostic CLI, analytical/native
references and Linux workflow. Independent safety review remains pending.

The preceding rigid Deposit check cannot approve a forming bead touching the
working face. Add a separate, explicitly declared synthetic model, not a contact
ID interpreted as permission. It owns version/identity, head revision/material
model references, working radius, recent arc-length wake, maximum permitted
material top above the face, gap/width ranges and path gradient. Production
profiles, projects, cache keys and the fixed mandatory job registry are unchanged.
The model is an assumption for simulation, not measured deposition calibration.

Require one complete maximal contiguous Deposit block with continuous XYZ and
co-oriented collinear nonzero XY vectors. Each constant-flux section must have
the same shape class, full delivered-width bounds and gap inside the declared
domain, and |dZ|/dXY within its gradient. Turns, reversals and partial blocks
refuse. For source height Lipschitz constant g and recent arc-length wake W,
g * min(total_run_length_upper, W) + original_Z_growth + numerical_error must
not exceed the declared top limit, which is positive and below gap_min. There
is no assumption about underlying support, old adjacent material or physical
bonding. Those require their own proofs.

Only the zero-thickness finite tool-local working face can use this model.
The opening and outer annulus boundaries remain closed as before. A working
radius smaller than the outer radius requires each accepted local cell wholly
inside that working disk. Equality uses the existing actual finite annulus,
not its square bounding corners. No nozzle sidewall, body, sensor, cooling
part, static object, pre-block bead or Travel uses contact permission.

For every padded whole swept cell, independently bound its projection onto the
current exact final-decimal XY vector. Subtract the latest cell front and the
original along growth/numerical error. Only if the resulting lower relative
arc coordinate is >= -W may the cell omit material of this exact forming run.
Collinear forward continuity makes this coordinate common to every selected
packet. Thus any same-run material capable of intersecting the cell is recent;
older same-run material cannot intersect it. Future material is absent by the
existing chronology rule. At zero front the current packet is empty. Other
material still passes the original exact Upper membership query, with every
original head/static margin. Whole time/local cells establish clearance; point
samples only establish rigid refusal at their actual simultaneous front.

The result privately owns its exact replay source, pre-block prefix, copied
scene/model and complete leaves with an explicit contact flag. Copy callers
before callbacks; latch source/material/rate/scene/contact staleness, exceptions,
cancel/deadline and floating environment failures. Original work/cell/depth
limits apply through both positive allocation and negative publication. Late
refusal discards proof and witness. Old Travel/rigid Deposit calls have no model
and retain their output contracts and work counts.

The diagnostic command is `--linear-forming-contact-geometry-only RATE MATERIAL
QUERY CONTACT CANDIDATE`. CONTACT is a strict version-1 16-field scalar document;
duplicates, missing/unknown fields, nested data, boolean numbers, invalid
identities and confirmation claims refuse. Parse/rate/material/geometry share
the original absolute one-second root deadline. Exit 0/2/3 is component
PASS/FAIL/UNKNOWN; job is always UNKNOWN and export false. No general contact
permission or certificate is generated.

Separate 113-bit final-text equations reconstruct dose/width/gradient/top
domain, validate every permitted leaf's finite working zone and worst age,
and check all nonpermitted Upper/head/static cells and complete partitions.
Restricted rectangle and rounded-section equations independently verify actual
negative witness enclosures under a common admissible growth translation.
No planner/verifier membership or broad-phase code is called by the oracle.

The actual owned candidate keeps every byte and original scene/error/margin.
Its 108-packet closed contour refuses because of turns. Its 53-packet straight
hatch fails against the earlier contour at its actual initial front. Its four
prospective nonplanar packets pass the declared model. Analytical examples
include working-face contact that the rigid mode rejects, full forward/reverse/
rotated/rising/descending/varying-gap runs and old-neighbor/head/static negatives.
Do not broaden the model to convert the contour/hatch refusals to PASS.

Still required: qualified support/contact with old support and neighboring
lines, curved/closed contours and seams, complete fixed-width cap fill, all
earlier/printing/prolog/parking/end motions, complete source/software/transform/
delivery qualification and full job publication. The mandatory registry remains
4 PASS/RUN and 13 UNKNOWN/NOT_RUN; export stays BLOCK. Physical U1 qualification
is separate. Standard head and 0.4 mm nozzle are known; firmware version and
plastic brand do not gate software implementation.
