# ADR-0071: independent final-byte material reconstruction

Status: accepted for declared synthetic constant-flux sections; complete B12 open.
Parents: ADR-0068/0069/0070. Normative SPEC/DATA_CONTRACTS remain unchanged.

The independent verifier reconstructs every final event from its owned bytes.
Explicit source declarations bind original IDs/order/poses, expected nominal
amount and fixed-gap rectangle/rounded-rectangle section parameters. They are
inputs, not planner collision/support flags. Changed rounded XYZ and E recompute
XY length, area and width; the old width or geometry certificate is not reused.

Nominal volume is final E*pi*d²/(4*k). Delivered volume is a declared interval
nominal*(1 +/- relative_error) +/- absolute_error, clipped below at zero.
Uncertainty is a simulation assumption, not calibration. The accepted model has
constant flux throughout each event, so partial volume scales with actual event
progress. Pressure/Travel/dwell add no material; a final retracted state remains
permitted without invented restoration. Shape parameters do not prove that the
specified gap is supported by the actual preceding material.

Source pose deviation is checked in the complete 3D vector. Per-record and
whole-sequence absolute nominal-dose deviation have explicit budgets. Exact
rational comparisons decide violations; pi, sqrt, widths, coordinates and totals
have outward enclosures. The rounded section requires area > pi*h_max²/4 for
all admitted dose values. Unsupported or uncertain sections cannot grant PASS.

Original model growth, erosion and coordinate uncertainty remain immutable.
Nominal/delivered width intervals describe section parameters before erosion.
Outer boxes are broad-phase bounds only; they are never filled support solids.
A finite inner butt interval that is empty is explicitly recorded. Full lower
shape membership, union/coverage/contact and continuous head clearance remain
separate mandatory work. No old geometry proof qualifies the new rounded ledger.

Protected full/prefix snapshots own exact internal states and all inputs. Prefix
replay includes every completed deposition and only the current fraction, with
no future/pressure material. Stale source/rate/material policy, cancellation,
rounding/underflow, bytes/record/work/deadline limits and final publication refuse
without a partial snapshot. Work starts at the preceding proof's cumulative
count; resource checking remains cooperative, not a hard process sandbox.

Shared Exact.hpp arithmetic is limited to verifier components. Planner/writer/
GCodeProcessor decisions remain absent from the separate library/command.
Original material/IR/profile/3MF/fingerprint schemas and guarded export remain
unchanged. Runtime material reconstruction is version 1 with a new explicit
source declaration, not a migration of existing Orca projects.
