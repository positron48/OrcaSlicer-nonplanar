# A04 critical review — 2026-09-28

Separate author review of the numerical/geometry implementation. This is not
an independent second reviewer; A08 independent review remains pending.

## Bounds and decisions

- The annulus extremum uses its outer radius and the full plane gradient.
  The affine centre path makes the endpoint minimum sufficient for this pair
  only. This shortcut is not used for the box sweep.
- A box interval encloses the position for every parameter in its interval.
  Monotone min/max and outward arithmetic then enclose signed clearance.
  PASS requires a strict lower-bound comparison on a covering partition of
  [0,1]. A sample can establish a violation, but can never establish PASS.
- A FAIL upper bound must come from an actual sampled pose (or a plane endpoint).
  The initial box interval upper bound is not used to manufacture a witness.
  Global lower bounds are retained even when a later evaluation exhausts work.
- Input uncertainty is charged in full in both directions. Arithmetic rounding
  is already included in the distance bounds. Zero requested clearance does not
  accept a tangent/overlap, and uncertainty is never clamped away.
- Timeout, work exhaustion, unsupported pair/contact, invalid input, numerical
  overflow and unsupported rounding/underflow modes retain UNKNOWN. No export
  decision or Verified state is exposed by the query.

## Changes made during review

- Separated the first actual witness from the initial interval upper bound;
  a broad interval alone must never fabricate a collision pose.
- Set strict floating-point options on this source only and excluded it from
  precompiled headers so MSVC does not combine incompatible /fp modes. Added
  runtime nearest-rounding and gradual-underflow precondition checks.
- Added 300 seeded scenes checked with an independent long-double slab oracle,
  plus a thin off-centre wall and a diagonal route that needs subdivision to
  prove clearance. Fixed analytical cases check finite tip, full gradient,
  Euclidean box separation and exact positive/negative uncertainty margins.
- The first native run found a premature UNKNOWN at seeded scene 29: depth-first
  subdivision exhausted a tangent interval before visiting a queued penetrating
  interval. Changed the work queue to visit the smallest lower bound first;
  kept the independent oracle and negative case unchanged. This was a missed
  definite FAIL, not an unsafe PASS. Original failing evidence is retained.

## Limits that remain

Only the declared two pairs, fixed axes and straight segments are covered.
No complete profile/scene validation, rotating/moving scene geometry, cones,
meshes, sphere pair, deposition-contact or sequential deposited material exists
here. Strict arithmetic is locally exercised on macOS ARM64; Linux/Windows and
alternate compiler/runtime modes need platform evidence before qualification.
The query is used by native tests, not yet by a production planner or byte
verifier. A05/A06/A07/B05/B08/B12 and the rest of Gate A/P2 remain separate work.
