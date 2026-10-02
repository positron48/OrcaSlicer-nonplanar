# B12 independent final-byte linear rates

Invariant: ADR-0070. A separate C++17 library and headless command check owned
final decimal bytes against exact policy values without planner/writer/
GCodeProcessor decisions. The native adapter binds the original complete B11
candidate and all motion limits, cumulative work and publication guards. This
component does not grant whole-job verification or export.

## Observed result

macOS ARM64, Apple Clang 21.0.0, Release. Build1 compiles application and both
consumers in 418.767 seconds; final build6 completes application, both consumers
and the standalone command in 18.660 seconds. Strict-command evidence shows
LinearRates.cpp uses -fno-fast-math/-ffp-contract=off without PCH. Registered MSVC
flags are /fp:strict. Standalone link evidence contains only its object and the
independent library, without the slicer or GUI libraries.

Seven new cases / 137 assertions pass (rates5, 0.041 seconds): exact decimal
boundaries, all limit mutations, Cartesian/CoreXY difference, short triangular
peak, command/nominal dose and ideal time, pressure/dwell, stale/cancel/resource/
rounding/publication guards, caller mutation and protected native binding.
Ten final CLI cases pass (cli3, 0.472 seconds), including retained final
retraction, explicit component exits, duplicate/unknown keys, excess array
coordinate, measured claim, unknown pure-E purge and nonregular input. All reports
keep job UNKNOWN/export false; input byte hashes remain unchanged.

Final native case: 1 / 75125, 8.323 seconds. All 2218 original rows (2092
depositions, 126 Travels) pass the production independent rate component.
The owned 128890-byte candidate is unchanged from B11, SHA256
`d31bc1ef4ec3c4c1342d0ec2ecd89818749acf52a33ce77a1f149d32f32220a8`.
Cumulative rate work is 66507 in the recorded diagnostic native run; ideal
mechanical duration is approximately 1383.7 seconds, not actual printer time.
Native pressure/dwell rows are absent; separate analytical and CLI fixtures
exercise them. The independent 113-bit test formulas remain a separate oracle.
An additional native4 run saves the exact current candidate/sidecar (75131
assertions, 9.508 seconds). The separate process checks those same bytes in
0.173 seconds, work 28746; together with candidate work 37761 this matches the
adapter's 66507. Its ideal time interval is [1383.7042310499478,
1383.7042310499482] seconds. Explicit input/report hashes bind this diagnostic
evidence; they do not implement the complete-job manifest.

Final legacy/candidate/parser selection: 12 / 1524, 0.062 seconds. Material:
122 / 48037, 39.643 seconds. Body/cap: 14 / 183013, 35.947 seconds. Selected
CTest: 378/378, zero failures/skips, 93.115 seconds gate wall time. Six fresh
strict OFF/ZAA pairs pass; original IDs and timestamp-only normalization remain,
with only the established additive OFF-default allowance. Source/package and
authored whitespace checks are recorded separately in the archive.

## Numerical and state boundaries

Rate decisions use exact rational squared inequalities. Pi/dose and sqrt/time
have explicit outward enclosures. Double parsed moves are diagnostic values,
not the exact geometry oracle. Identity transforms, known initial state and
ideal full stops are declared synthetic preconditions. k is not applied twice;
pressure/dwell produce no deposited material. Actual delivered volume,
pressure advance and motion applicability remain unqualified.

Event frequency uses the complete triangular/trapezoidal time. The existing
generator's stricter speed cap is unchanged. Final retraction is allowed as in
ADR-0069; the snapshot/CLI report retains enclosed pressure debt. Unequal
restoration, repeated retraction and deposition during pressure debt refuse;
no automatic restore or phantom bead is added.

The native rounded recapture preserves every original material row, all seven
head parts, Z=0 tip, .01 mm margin and all original losses. Additional conversion
allocation is uncertainty. The original zero-conversion source still refuses
serialization and the complete original B09 exit stays UNKNOWN without route.

## Retained history

The initial missing-header build fails before implementation. The first four
cases were added after the implementation scaffold; their first pass is not
represented as behavioral test-first evidence. CLI-red fails because the
standalone executable does not yet exist. CLI-array-red proves a fourth axis
value is silently ignored by generic JSON array conversion; explicit size
admission corrects that boundary. Cadence-red exposes using peak/length instead
of complete event time; the new production formula fixes it.

Pressure-red initially assumes final debt must be zero. Its subsequent
implementation was stricter than ADR-0069, which explicitly permits final
retraction. Final-state-red reproduces that accidental regression; the final
implementation preserves the original allowed state and reports exact debt.
These new test assumptions and both red logs are retained, without changing
any original negative, source admission or margin. Builds remain sequential;
tests never use a failed-build binary. Native2 selects the wrong test executable,
matches zero cases and exits 2; native3 runs the real case in fff_print_tests.
The empty run is preserved and is not counted as verification.

## Provenance and remaining scope

Runtime rate contract is 1; only this new diagnostic policy/report is added.
Original profile/3MF/material/scene/IR/cache fingerprint formulas remain.
Configured software label 69c4b339 is the parent, not this implementation;
archive source/dependency/four binary hashes bind the actual tested build.
CLI reports alone have no complete manifest/software/scene/job hash binding.
Complete B13 binding and containment remain pending.

The 07:44 UTC saved Linux observation confirms B10 900cc58a / run 36968335656
and B09 89f009b9 / run 36965840048 succeeded; B11 69c4b339 / 36973396044 is in
progress. Current B12 requires its own CI run after publication. Windows,
independent safety review and physical qualification remain NOT_RUN/pending.

Full B12 rounded material/contact/support, dose uncertainty, complete scene and
route reconstruction, audited filters/prolog/end motions and job integrity are
not implemented by this rate component. Native cap/contact/access/order work and
original B09 exit UNKNOWN remain. Guarded export stays BLOCK; full B01–B15 stays
IN_PROGRESS. Software proceeds independently of firmware/material-brand questions.

Implementation commit: `git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B12-final-rates.md`.
