# A03 critical review — 2026-09-27

Separate author review pass against the actual C++ implementation, native test
output and untouched stock capture. This is not an independent second reviewer;
the A08 independent safety review and overall Gate A acceptance remain pending.

Findings resolved during this pass:

- Made the Eigen decoded coordinate vector an explicit `Vec3d` rather than
  retaining an expression object at the adapter boundary.
- Added 2000 deterministic samples across the declared coordinate domain to
  check the Euclidean error bound, in addition to exact/nonfinite/boundary cases.
- Baseline comparison now requires all six distinct fixture/mode pairs; repeated
  manifest entries cannot replace a missing positive ZAA case.
- Dev bundle entry point now enforces an explicit local datadir/write sandbox
  and network denial itself; a renamed executable alone would permit unsafe
  fallback to stock configuration. Removed the unused earlier bundle copy.
- Corrected A04's test-ID mapping to the original acceptance CSV.

Checks and mathematical boundaries:

- Coordinates are finite frame-specific doubles; conversion checks magnitude
  before cast. The native domain is below double's exact integer limit.
  Encoding rounds to nearest (ties away from zero), differing from native
  truncation by at most the explicitly charged grid error. New code does not
  read absolute Z from a relative ZAA offset.
- Error allowance includes three axes, scale representation and arithmetic.
  NumericBudget adds components conservatively upward. It excludes physical
  measurement/position/bead uncertainties, which must be added independently.
- Affine gap integral is area times center gap. The test's nonconstant gap has
  exact independent integral 3.4 mm3. Filament correction is applied once;
  neither gap-to-normal conversion nor 3D path length enters this integral.
- Payload types prevent travel/retraction from carrying a deposition volume.
  Nonzero IDs assert references only; they prove no support/contact/inclusion.

Unresolved, intentionally outside A03:

- NativeScale's equality check is not a lock. B01 must stabilize the native
  Print state and make stale result invalidation atomic; A03 has no production
  planner consumer and no concurrent global writes.
- A03 validates individual event structure. A06/B10 must implement ordered
  retraction amounts/state continuity, dynamic and volumetric limits and the
  actual writer boundary. No implicit frame transform is available.
- Affine cells are not arbitrary rounded beads/meshes. Accumulated volume
  error, ownership, contact and D_lower/nominal/upper inclusion need A04/A05/B05.
- Baseline replay compares final stock/fork bytes; it does not model firmware,
  material deposition or safety. Unsupported commands reject its limited replay.
- No new mode/export exists; there is no artificial PASS or export permission.
  UI/CLI/cache/3MF gate integration and physical qualification are NOT_IMPLEMENTED.
