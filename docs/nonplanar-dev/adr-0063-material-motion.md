# ADR-0063: continuous rigid tool motion against actual sequential material

Status: bounded software path implemented; complete head/scene/contact/job and
independent review remain pending. Scope: DepositionModel and both native
consumers. No global type, persisted schema, preset, IR, cache or export change.

## Invariant

A protected source is obtained by revalidating the original ordered material
ledger once through its existing capture factory. Recompute its derived geometry;
a forged public raw aggregate cannot supply a continuous-motion certificate.
Own source, tool components, policy and limits before callbacks. Charge the copy,
validation and canonical hash walks to preparation work and the common deadline.

The checked event is an original row in that owned ledger. Earlier deposition
rows are complete; the current deposition grows at the same parameter t as the
nozzle. Travel/retract/unretract add no material. Later rows are absent. Neither
nominal layer labels nor caller-injected trajectories/prefixes reorder this state.
Every supplied component must be rigid-forbidden. DepositionContact is UNKNOWN;
no previous/adjacent material or part of a nozzle is excluded by a contact rule.

## Continuous volume proof

For each supplied component and active deposition row, partition the complete
domain [0,1] x tool-local volume. Support complete ToolBox volumes and the existing
flat FiniteTip annulus, including its empty opening. A ring starts with an outward
bounding square; a spatial leaf is empty tool only if an interval radial bound
proves that it lies strictly inside the opening or outside the outer circle.
Overlapping bounding boxes never alone establish material collision.

For every other leaf, outward intervals enclose the original interpolated nozzle
position plus the complete local cell. Expand all axes by required clearance plus
numeric, tool measurement, positioning, material and scene uncertainty. This cube
contains the Euclidean clearance ball. Source numerical error is already in Upper
and joins the unchanged .05 mm numeric domain budget. PASS requires every retained
leaf to be continuously Outside the original declared D_upper predicate.

The existing rectangle/rounded constant-flux material predicate accepts interval
progress internally. For the current row use the whole leaf time interval, not
only its end: variable-gap/end inflation is not assumed monotone in the prefix.
Prior rows use progress 1. Ordinary point/coverage/roof callers retain the scalar
wrapper and the same distinct Nominal/Upper/Lower formulas.

Uncertain leaves split time or the longest local tool axis. Time split preference
uses physical motion and current gap variation; this affects efficiency, never
acceptance. Shared depth/cell/work/time/revision/cancellation/rounding limits cover
all pairs, predicates, radial tests, probes and publication. An incomplete proof
is UNKNOWN without a protected successful snapshot.

A negative-only probe must belong to the actual box/annulus and lie robustly
Inside Upper at its exact time, including positional/input uncertainty. It yields
FAIL with material row, component, original event/time and owned geometry/policy.
It does not estimate first contact. Probes never accept a motion. Clearance-band
ambiguity without a direct interior witness can remain UNKNOWN. The success
snapshot retains the full source, tools, policy and complete closed partition.

## Verification and remaining scope

Analytical positives/negatives cover rising/falling current fronts, interior-only
travel collision, complete boxes whose centres miss, flat annulus opening,
future exclusion, retract/unretract, both sections and an angled varying-gap bead.
113-bit checks independently bound whole-time forward separation, partition measure
and nominal interior witnesses. Budget/stale/deadline/rounding/ownership/final
publication negatives are retained. A forged derived ledger is recomputed.

Native body/first-cap/later material is consumed directly. A high simulation box
and annulus have continuous material proofs for the actual later packet and its
connector. A low rigid box intersects that current later packet; prior/future
geometry is not substituted. These tools do not describe a measured U1 head.

This API proves only supplied rigid components against declared ordered material
for one original event. Complete qualified head envelopes, static scene composition,
contact/side-floor rules, whole-job order/travel/entry/exit/parking, all motion/flow
limits, finite-byte independent replay, applicability and physical qualification
remain required. Public guarded export stays BLOCK; full B01-B15 is IN_PROGRESS.
