# B11 native decimal rates: critical author review

Date: 2026-10-03. ADR-0098. This is an author audit; independent safety review
remains PENDING. No subagent review was requested or represented as performed.

The preflight derives actual integer units from the same native emit_axis
rounding rule. Coordinate/E limits and digits≤9 keep each lattice integer and
coordinate difference below2^53. Later deltas use integer differences instead
of subtracting separately enclosed large positions. The first actual pose uses
the binary64 initial_position that the verifier receives. Outward binary64
operations bound length, axis/drive and E ratios; the existing enclosed π bounds
actual commanded volume. Arithmetic requires nearest rounding and gradual
underflow at every guard. Final commands are rounded downward; XYZ/E formatting
is unchanged. No independent verifier implementation is called by the producer.

All rate corrections are minima with the original planned ceilings and policy
initial acceleration. The single global M204 obeys every non-dwell row. Feed
ceilings bound actual peak even when the lower global acceleration changes the
triangular/trapezoidal regime. Speed≤length×event frequency implies a conservative
minimum interval; original dwell rounding is retained. Pressure E contributes
no volume, preserves original paired amount/state, and remains subject to final
strict length/state/rate checks. Cross-section and material dose cannot be
corrected by feed changes and remain independent final checks.

The immutable plan/policy/limits are captured before callbacks. Every row and
axis loop charges the original cumulative budget; byte appends and final
publication retain their guards. Intermediate commands have at most200000 rows;
there is no partial snapshot after cancellation, staleness, exhaustion, collapse
or callback/arithmetic failure. The tests exercise the exact successful work
threshold and the immediately insufficient threshold, plus existing cancellation
and final-publication failures. Hard RSS/host callback containment remains open.

The test-first mirror connector failed the unchanged exact final verifier with
AXIS_ACCELERATION_LIMIT before the patch. Positive primitive tests then pass
Cartesian/CoreXY and mirrored coordinates; restoring old ceilings produces
strict axis/drive/filament/Q refusals. Coarse E changes material dose, which is
intentionally outside the rate component's PASS. A reduced cross-section policy
still refuses. Actual full later analysis under original100 preserves all prior
XYZ/E/order/barriers. An80-digit Decimal witness and actual separate native
auditor certify the rate correction and retain both old-byte negatives.

The patch changes only the full-stop candidate producer and tests/oracles.
Shared stock formatter and OFF/ZAA paths remain unchanged and require actual
regression evidence. Request/plan/manifest and policy versions remain. Existing
software identity invalidation binds changed compiled sources; full software
qualification, filled caps, seams/contact/head/routes/order/whole job/import/3MF,
GUI, Windows/Linux and physical qualification/publication remain open. No rate
or simulation result authorizes export or physical printing.
