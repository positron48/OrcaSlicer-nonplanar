# A08 source-based critical audit

Reviewed base: c044c4b6, plus the feed-rounding correction in this milestone.
Scope: Contracts, Interval, Collision, Transition, GCodeAdapter, independent Replay,
ProfileScene, native tests, CMake flags and actual production call sites.
This is an **author audit**, not independent reviewer approval. Conclusions below
come from source and executed tests, not from prior milestone success summaries.

## Findings

| ID | Finding / evidence | Disposition |
|---|---|---|
| A08-R1 | GCodeAdapter accepted input F=99999.9996, but native formatting produced F100000 outside Replay's strict F<100000 domain. The new red test failed before the patch (exit 42). | Fixed by checking the native quantized value before returning a candidate; both the rejected boundary and F=99999.999 positive neighbour pass. No margin or parser range was weakened. |
| A08-R2 | Collision implements finite-tip/plane and translating box/box only. The sphere fixture STL is not an analytical sphere clearance oracle. SPEC 28 explicitly lists analytical plane/sphere evidence at Gate A. | Gate A open. Add a separate analytic sphere domain/oracle task; unsupported primitive pairs must remain UNKNOWN. |
| A08-R3 | No prepared physical coupon with a documented later execution protocol is recorded. Existing generic model fixtures alone do not fulfil this Gate A item. | Prepare a coupon artifact/protocol without authorizing a printer run. Real dimensions and operator qualification remain separate. |
| A08-R4 | Separate independent critical review has not been obtained. | A03–A07 approval stays pending. This document does not certify the author's own code independently. |
| A08-R5 | New query/transition/serialization/profile APIs have native test consumers only; source search finds no production hybrid pipeline. | Expected P0 boundary. No P1 claim, big UI, export route or clearance badge. |
| A08-R6 | Replay handles bounded text/modal state, not geometry/material chronology, calibrated delivery, snapshot hashes or axis/flow/acceleration limits. The adapter's ordinary rounding budget is not a proof that rounded speed satisfies a future physical ceiling. | B06/B10/B12/B13 work remains. A08-R1 only fixes numeric dialect closure, not a motion limiter. |
| A08-R7 | Native serialization and replay cap at 10000 moves. SYS-08 asks for 200k facets + 200k moves through the full pipeline. | SYS-08 NOT_RUN. Bounded benchmarks below cannot be extrapolated into a full-cycle PASS or justify weakening these limits. |
| A08-R8 | Profile role/inventory/moving-envelope fields are synthetic assumptions. Relabelling or confirmation is rejected; there is no measured-profile loader or validation pipeline. | A07 operator worksheet remains UNCONFIRMED, all measurements null; PRF acceptance only covers the documented native subset. |
| A08-R9 | macOS native results exist; Linux, GUI coexistence and current production packaging are unverified. | Keep platform/GUI limitations explicit. Earlier OFF/ZAA CLI captures are historical; new shared slicing/writer algorithms are unchanged. |

## Mathematical and integration checks

- A03 uses named scaled-coordinate boundaries and distinct volume, filament and
  material identities. Scalar typed inputs are structurally finite; IDs alone
  do not prove support or material containment. No numerical contract changed here.
- A04 tip-plane distance uses the full planar gradient and outer tip radius.
  Endpoint extrema are justified by the affine plane, not generic sampled-only
  collision checking. Box/box bounds enclose whole parameter intervals; a priority
  queue avoids starving later penetration. Unsupported pairs/contact, budgets,
  rounding modes, deadlines and resource exhaustion return Unknown.
- A05 derives an eroded flat core from actual native solid-infill paths and rejects
  ZAA/sloped/bridge/force-no-extrusion paths. Nominal and uncertain volumes remain
  distinct. The affine rectangle integrates all corners. This proves neither
  multi-bead seams nor chronological material reconstruction or a full cap solver.
- A06 initializes a fresh writer with zero additional plate offset and preserves
  input sequence and absolute XYZ. The independent STL-only parser restores
  omitted Z/F and checks actual text. The new 10000-move test independently
  checks endpoint and total E, including the sum of decimal quantization bounds.
- A07's interval Minkowski envelope covers its entire convex nozzle domain.
  Endpoint membership proves straight-segment domain containment only. A positive
  fixture intentionally includes a colliding obstacle: applicability cannot be
  confused with collision clearance. Moving-part/omitted-upper coverage remains
  conditional on synthetic declarations, not machine observations.
- Collision/Transition/ProfileScene retain strict arithmetic flags and disabled
  PCH for interval arithmetic. Shared writer, stock slicing and ZAA source were
  not changed. No PASS/ALLOW/export stub, relaxed margin or removed negative test
  was introduced. No profile, cache or persistent schema changed.

## Native hooks retained for subsequent work

1. Capture immutable resolved settings/model placement before partitioning;
   production revision/hash binding remains to be implemented.
2. Use native cut_mesh and a derived Model before Print::apply/process for the
   bounded reservation experiment. General partition geometry needs its own tests.
3. Read semantic native ExtrusionPath data for deposited-base reconstruction;
   do not infer support from CAD or parse role comments as the planner API.
4. Use low-level GCodeWriter absolute XYZ plus explicit volume-derived E;
   do not enter GCode::_extrude's ZAA deformation branch. Apply future motion
   constraints and filters before independent final-byte replay.
5. Keep Replay independent from planner decisions and GCodeProcessor; add the
   missing geometric/material/manifest checks before a shared export gate.

Raw source-consumer inventory is retained with the evidence. No runtime scene,
printer connection, firmware change, macro or user preset was accessed.
