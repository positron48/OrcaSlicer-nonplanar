# ADR-0069: owned native full-stop candidate and independent byte replay

Status: accepted for bounded synthetic simulation; complete B11/B12 pending.
Parent: ADR-0068. Normative SPEC 19/20, DATA_CONTRACTS 6 and B11/B12 are unchanged.

## Invariant and allowed scope

An immutable protected B10 complete-journal plan produces owned final native
bytes, a SHA256, exact candidate-policy fingerprint and one byte range for every
original event in original order. No filtered/reordered/substituted journal,
implicit Z reset/wipe/parking or phantom deposition during pressure restoration.
No partial candidate is returned after cancellation, stale revisions, numerical
failure, collapsed commands, resource exhaustion or late publication refusal.

The producer uses the existing native GCodeFormatter with isolated configurable
1..9 digits (default XYZ/F/A six, E nine, dwell three). It leaves stock formatting,
GCodeWriter, GCodeProcessor and upstream export paths unchanged. The adapter is
compiled without PCH and with strict floating arithmetic on all registered
platforms. Legacy A06 replay and its unsupported-command negatives remain.

## State, acceleration and pressure

Synthetic preconditions are a known starting position and initial acceleration,
millimetres, identity transforms/overrides and the declared full-stop model.
The output starts G90/M83/M400 and emits one M204 S value rounded downward below
both the known initial acceleration and every non-dwell step ceiling. Each XYZ
command explicitly includes all three axes and F. Pure-E commands only perform
a balanced Ready -> Retracted -> Ready pressure cycle. Stationary Travel gets an
upward-rounded G4 P dwell. Every original event ends with M400.

These are candidate commands, not installed firmware qualification. The
[primary Klipper command reference](https://www.klipper3d.org/G-Codes.html#g-code-commands)
supports the command spellings and units. The
[primary toolhead implementation](https://raw.githubusercontent.com/Klipper3d/klipper/master/klippy/toolhead.py)
shows M204 changes global acceleration. Consequently emit one lower value and
never raise it or restore it to a guessed value; do not treat M204 as a command
that preserves the current global limit automatically. The reference was read
2026-10-02. No machine connection/configuration change or physical execution.
The lowered global acceleration changes timing: the B10 timing bounds are not a
certificate for these bytes. Complete final-byte rates/time must be reverified.

E uses original V, filament area and k exactly once; signed pressure amounts
add no nominal material. Tiny rounded XYZ/E/F/A commands refuse, not disappear.
Unbalanced pressure restoration is already refused by material-source capture
and is checked again during serialization. Final retracted state is allowed; no unrequested restoration is invented.

## Rounding and independent consumer

A full vector coordinate error bound contains the half-quantum plus a conservative
floating conversion allowance over the admitted +/-10000 mm domain. It must fit
the original scene numeric conversion allocation. A zero-allocation source cannot
produce a new rounded candidate. The native positive recaptures every original
row/head with an additional explicit 2e-6 mm allowance, preserving the original
.01 mm clearance and all other material/scene fields. This is extra uncertainty,
not a reduced margin or qualification of rounded material/contact geometry.

FullStopReplay.hpp is a separate standard-library-only parser/state machine with
no planner, writer, GCodeProcessor or certificate calls. It reads copied final
text, demands the fixed header, one acceleration, complete XYZ/F, balanced
pressure and per-event barriers, bounds bytes/events/time, and refuses malformed
numbers, truncation, unsupported commands and non-nearest rounding. Its result
is parsed moves, not a safety PASS. Test-only independent 113-bit formulas check
all axes/drives/E/Q/cross-section/pressure/event rates on those parsed values;
they do not implement the complete material/contact verifier.

## Ownership, identity and publication

Plan, policy and limits are captured before callbacks. Every record walk/append
and final hash/publication charges the source cumulative work; this call has one
absolute deadline. Material, scene, both motion/candidate policy revisions,
cancellation and rounding are checked throughout and after construction.
Exact fingerprint includes version, IDs, known initial acceleration and every
precision setting. Immutable bytes and hash are owned together; any downstream
byte mutation requires a new verification/job binding. No final-filter framework,
persisted cache, output file, new 3MF/profile schema or export approval is added.
Full B13 end-to-end resource/software/job provenance remains pending.

Complete independent final-byte material/contact/support/axis/rate/time proofs,
audited native filters and prolog/end motions, firmware/flow applicability, job
integration, independent review and physical qualification remain required.
