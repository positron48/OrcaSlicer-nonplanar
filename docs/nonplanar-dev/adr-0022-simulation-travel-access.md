# ADR-0022: owned full-head travel checks in a declared box scene

Status: implemented and native tests pass; independent review pending.

Connect A07 coverage/inventory checks to A04 continuous primitive queries for
Travel events only. Capture the scene, chronological events, policy and limits
before callbacks. Require SimulationOnly applicability, then check the finite
annular tip and every head envelope against every declared static box for every
event. No pair, component or event may be skipped on a successful result. An
explicitly complete empty inventory has zero pairs, still SimulationOnly.

The scene's uncertainty applies to each geometric envelope. Relative clearance
must therefore include twice that bound, accumulated outward in a separate
scene_geometry policy term, in addition to the caller's numeric/measurement/
positioning/material terms. Geometry contract version 4 names this extra budget;
its default is zero, preserving prior callers. Coverage already expands both
head and obstacle envelopes separately; it is not a clearance certificate.
The travel check additionally inflates coverage by the entire caller budget and
required clearance on each envelope. Otherwise unknown geometry just outside
the declared scene could invalidate a pairwise or even empty-inventory PASS.

The tip gets the smallest unused positive component ID; the result records it.
Obstacle IDs are one-based indices in the captured scene. Existing head/event
IDs are retained. Keep only a limiting/failed pair plus pair counts; partial
diagnostics do not grant PASS. Bound the total pair product, each primitive's
evaluations and one overall cooperative deadline. Recheck cancellation and
profile/revision before publication. Deposition/retraction and evolving material
remain UNKNOWN; this is neither job ownership nor physical/export qualification.

Predefined positive scene: six compact head envelopes, a wider asymmetric duct,
annulus ri=0.2/ro=0.5, nozzle travel (11,10,2)->(11,20,2), low floor plus remote
box. Expect all 7 x 2 pairs checked. Add a box only in the duct's swept interior:
tip and endpoints stay clear but the whole-head result must fail with the duct
ID. Raise the floor so tip gap is 0.25 with required 0.1 and scene uncertainty
0.1 per envelope: relative uncertainty 0.2 must prevent PASS. Callback mutations,
late revision/cancel, empty/incomplete inventories and resource limits are tested.
