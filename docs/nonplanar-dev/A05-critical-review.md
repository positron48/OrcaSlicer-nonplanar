# A05 critical review — 2026-09-28

Separate author review of reservation, support and numerical decisions. This
is not an independent second reviewer; A08 independent review remains pending.

## Geometry and material

- Reserving the body before Print::apply makes native shell generation operate
  on the body mesh. Mutating late fill surfaces would not establish this: native
  perimeter generation can restore surfaces. The analytical cut checks both
  volume conservation and preservation of the source mesh.
- Lower support uses only the flat width w-h of a rounded-rectangle bead, with
  inward endpoint/lateral erosion. Full width w is not guaranteed flat support.
  Explicit layer top, scale and object translation are required; no CAD surface
  or expanded collision envelope is substituted for an actual native path.
- The native integration fixture supplies a synthetic translation to physical
  coordinates. It does not qualify production placement or firmware transforms.
  The two independently sliced terrace interiors do not establish seam coverage.
- This conditional bead model does not establish real extrusion accuracy,
  adhesion, material chronology, or support over a union of neighbouring beads.
  Those remain later material/transition work. Width 0.8 mm in the test is a
  simulation fixture, not a qualified nozzle setting.

## Bounds and decisions

- Affine gap extrema occur at rectangle corners; all four are checked, including
  the derived fourth corner and its three-input error propagation. A centreline
  check would miss the transverse violation retained in the tests.
- Whole-footprint coverage by the eroded core is required. Uncovered shoulders,
  eroded endpoints, sparse/bridge/scarf/contoured paths and changed flow cannot
  manufacture a compatible result. Touching a thickness limit remains UNKNOWN.
- The affine integral uses XY area and mean gap. Its separate nominal and
  uncertain bounds never add material merely because uncertainty increased.
  Signed values for invalid cells are diagnostics, not deposition instructions.
  The first-pass strip consumes only part of the reserved cap budget; completing
  the entire cap or handling arbitrary model topology is not demonstrated.
- Shared Interval.hpp preserves A04 outward operations and runtime assumptions.
  Both consumers use strict source flags. A04 regression must still pass after
  the extraction; tests do not use the implementation as their volume oracle.

## Review changes

Added native FlowError handling and nozzle conversion checks so invalid float
flow inputs fail closed rather than escaping or becoming zero/infinite nozzle
diameters. Added coordinate checks for uncertain support and the derived fourth
corner, plus negative tests. Added reversal and translated Y-axis cases to cover
the second supported direction and explicit frame translation.

Native tests initially exposed a fixture configuration error: solid infill uses
solid_infill_direction, not the sparse infill direction. Kept the algorithm's
axis-alignment rejection and configured the intended analytical fixture. Its
volume oracle now integrates actual translated inputs rather than comparing an
ideal decimal rectangle to a tighter binary64 interval. No bound was widened.
Randomized FFF order also exposed the fixture's unset Print printer-identity
flag; both new fixtures now explicitly set the non-BBL identity supplied by the
normal CLI/GUI owner. Existing upstream empty-section diagnostics remain failures.

The shared OFF baseline exposed uninitialized object labels. The separate
091c6461 correction moves ID assignment before serialization for both exclusion
states, with a deterministic poisoned-ID test. Full baseline comparison was
rerun after the correction; no extra normalization was added to the comparator.

## Remaining boundaries

No production planner/exporter calls this API. Compatibility covers only the
declared cell gap and support, not tip/head/scene clearance, motion limits,
ordering, contact exceptions or export approval. The public internal structs
are not a trust boundary or persistent cache schema; later consumers must bind
them to immutable provenance and invalidate them with their job snapshot.
Linux/Windows, CI, physical measurements and printing remain NOT_RUN.
