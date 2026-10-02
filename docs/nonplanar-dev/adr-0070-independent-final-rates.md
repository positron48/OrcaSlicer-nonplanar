# ADR-0070: independent exact final-byte linear rates

Status: accepted for bounded synthetic simulation; complete B12 remains open.
Parents: ADR-0068/0069. SPEC 18–20 and DATA_CONTRACTS 3/4/6/7 remain unchanged.

## Invariant and scope

An independent C++17 library reads owned final decimal bytes and an explicit
synthetic policy. It checks the original straight motion domain, Cartesian or
CoreXY axis/drive speeds and accelerations, E speed/acceleration, retraction
length, commanded-equivalent Q/cross-section and event frequency. It reports
enclosed command/nominal volumes and ideal rest-to-rest time. A protected const
snapshot owns bytes, initial state, policy, parsed moves, bounds and work count.
No planner, writer, GCodeProcessor or production motion-decision helper is linked
into this library or its separate headless command.

Supported preconditions are known initial position/acceleration, millimetres,
identity transforms/overrides, synthetic unconfirmed profile and a full stop at
each event. Firmware lookahead and measured/operator-confirmed claims return
UNKNOWN. Material geometry, support/contact, delivered-volume uncertainty,
actual firmware applicability and complete job verification are still pending.

## Numerical contract

After the independent fixed grammar/state replay, a separate scan decodes the
same final decimal words as exact rationals, with at most nine fractional
digits. Policy doubles are decoded from their exact binary64 significand and
exponent. Boost's
[cpp_rational backend](https://www.boost.org/doc/libs/latest/libs/multiprecision/doc/html/boost_multiprecision/tut/rational/cpp_rational.html)
provides arbitrary-precision rational arithmetic. It is used without expression
templates to retain values rather than references to temporary expressions.

Let L² = dx²+dy²+dz², F be the final feed in mm/s, and A the final global
acceleration. Peak speed is min(F,sqrt(A*L)). Projected peak comparisons use exact
squared inequalities; acceleration comparisons are exact as well. CoreXY drive
coordinates are dx+dy, dx-dy and dz. A pressure-only event uses |E| as its
distance. A convex axis domain contains the entire straight segment when both
endpoints are within its bounds; this is not a tool/material clearance proof.

Filament area is enclosed by Machin's identity and alternating arctangent-series
remainders, rounded outward to 24 decimal digits. Positive XYZ extrusion gives
command volume E*area, and nominal volume E*area/k. Q/cross-section decisions use
these bounds without applying k again. A proven over-limit lower bound is FAIL;
an interval crossing the limit is UNKNOWN. Pressure/dwell contribute no volume.
These volumes do not establish the physical delivered-volume model.

Square root and floating conversion only seed an estimate. Exact rational
comparisons and adjacent binary64 values enclose the result before publication.
FE_TONEAREST, binary64 and gradual underflow are required; changed rounding or
flush-to-zero returns UNKNOWN. Strict compiler flags and no PCH preserve this
boundary. Zero/overflow/uncertain numerical bounds cannot grant PASS.

Ideal time is L/v_peak+v_peak/A, including both acceleration phases, pressure,
Travel and dwell. Event cadence uses this whole time: triangular events satisfy
16*r⁴*L² >= A²; trapezoidal events compare L/F+F/A against 1/r exactly. The
older generator's more conservative peak/length cap is unchanged. This is a
separately declared ideal mechanical model, not a bound on actual printer wall
time; firmware planning and M400/communications can add delays. Neither the old
B10 time bounds nor test-only rate formulas certify these final commands.

Retraction/restore state is checked again with exact E. Restore must match the
existing debt, and deposition while retracted refuses. ADR-0069's allowed final
retracted state remains allowed: report an enclosed final pressure debt without
inventing restoration, deposition or an epilogue. Whole-job completion stays open.

## Ownership and budgets

Bytes, initial state, policy and callbacks are captured before callback use.
Bytes/events/work/time have bounded admission and one absolute call deadline;
parser, exact comparisons, all event walks and final publication share the work
counter. Cancellation, stale policy, callback errors and final publication
invalidation return no snapshot. Constant-size pi construction remains bounded;
this is cooperative checking, not a hard process resource sandbox.

The native adapter maps every original protected motion limit, candidate initial
state and exact bytes. It retains source material/scene/both policy revisions,
starts with the candidate cumulative work and verifies the final record count.
Caller mutation cannot replace the owned source. No persisted cache, profile,
3MF, material schema, original IR or ledger fingerprint changes.

## Separate command and remaining scope

`nonplanar_rate_audit --linear-rates-only POLICY.json CANDIDATE.txt` reads bounded
regular files and requires all 23 policy fields, unique keys and exact three-axis
arrays. Component exits are 0 PASS, 2 FAIL, 3 UNKNOWN; usage is 64. The output
always has job_status UNKNOWN and export_allowed false. Exit 0 in this explicitly
selected numerical mode is not a verified-job or guarded-export exit. Inputs
remain untouched; no printer/config/network/publication operation is introduced.

The CLI report is a numerical diagnostic, not a persisted complete-job
certificate: complete input/software/scene/manifest hashes and final export
binding remain B13 work. Parsed double moves are diagnostic approximations;
future geometry must use exact decimal values or propagate conversion error.
Full rounded-material/contact/support and dose uncertainty, actual transforms,
complete filters/prolog/end routes, native exit UNKNOWN, independent review,
platform evidence and physical qualification remain separate obligations.
