# B12 final-byte material reconstruction

Invariant: ADR-0071. The independent verifier reconstructs every final event's
actual decimal poses, nominal/delivered dose, XY flux and varying rectangle or
rounded section width. Source IDs/order/poses and per-record/total dose budgets
are checked independently. Original growth, inner losses and coordinate error
are retained. Full/prefix snapshots are protected; pressure adds no material.
This does not certify complete material geometry/contact/support or export.

## Observed evidence

macOS ARM64, Apple Clang 21.0.0, Release. Initial missing-header build exits 1
before implementation (172.702 seconds, no tests run). Build1 completes application,
both consumers and headless command in 267.114 seconds. Final behavioral build5
passes in 3.457 seconds. Final build6 changes only helper indentation, completes
in 7.471 seconds and produces all four binaries byte-identical to the tested
build5. Saved preformat hashes and format-binary-audit.json prove that identity.
Strict compile evidence covers both verifier cpp files without PCH using
-fno-fast-math/-ffp-contract=off; registered MSVC policy remains /fp:strict.

Material-specific cases: 4 / 106 assertions. Rates: 8 / 144, including the new
late-rounding regression. Complete legacy/candidate/parser/rate/material selection:
24 / 1774, 0.076 seconds. Material core: 122 / 48037, 40.178 seconds. Body/cap:
14 / 183026, 36.663 seconds. CTest executes 383/383 with zero failures/skips,
95.811 seconds gate wall time. All six fresh strict OFF/ZAA comparisons pass;
original object IDs, timestamp-only normalization and the established additive
OFF-default allowance remain. Original package/source audits pass separately.

Complete native source: 2218 records, 2092 depositions and 126 Travels. Final
native3 with software evidence output passes 75146 assertions in 8.563 seconds.
The actual last deposition's half-progress prefix excludes unlaid material and
has strictly less nominal volume than the complete prefix. All original model
parameters are checked against the reconstructed policy. Native pressure/dwell
are absent; analytical and CLI fixtures cover them separately.

The current candidate is still 128890 bytes, SHA256
`d31bc1ef4ec3c4c1342d0ec2ecd89818749acf52a33ce77a1f149d32f32220a8`.
The protected current run saves all source declarations in native-material.json;
exact bytes/declarations match the earlier successful capture. The separate
process reads those same bytes and declarations, checks all 2092 depositions,
and returns component PASS/job UNKNOWN/export false (native-cli2, 0.287 seconds,
work 76886). Cumulative nominal amount is [3160.5020836242807,3160.502083624281] mm3.
It is not the measure of the overlapping material union.
Maximum nominal deviation from one original amount is at most
1.2018708202277373e-9 mm3, below its explicit 5e-9 mm3 budget. Total absolute
nominal deviation is [1.305669894012222e-6,1.3056698940122232e-6] mm3, below
1e-4 mm3. These are serialization/model calculations, not measured print volumes.

The native simulation declares 2% delivered-dose uncertainty, producing
[3097.292041951795,3223.7121252967668] mm3. That assumption is not material
calibration. Original numerical conversion allocation, all seven head parts,
Z=0 tip, .01 mm margin, original losses and complete original rows are preserved.
The original complete B09 exit still returns UNKNOWN without route snapshot.

## Mutations, guards and retained failures

Ten material CLI cases and ten existing rate CLI cases pass. Changed XYZ/E,
missing/duplicate owner, extra planner flag, confirmed-profile claim, extra axis,
changed pressure and duplicate key refuse. Every diagnostic keeps job UNKNOWN
and export false; input hashes remain unchanged. Mode --linear-material-only
requires explicit rate policy, complete material declaration and final bytes.
No presets, configuration, cloud, printer or production output route is changed.

Positive rectangle/rounded examples use independent 113-bit pi/width/volume
formulas, k=1.17, fractional current depositions and pressure/dwell exclusion.
Negative source/dose/pose/section/global budget, cancellation, stale source/rate/
material policy, work/records/rounding and final publication return no partial
snapshot. Caller mutation cannot replace captured declarations/options/source.
Unsupported zero XY extrusion or rounded-dose domains remain UNKNOWN.

Late-rounding-red reproduces the existing adapter's final callback changing
rounding after the child numeric guard (exit 42). Shared final source guard now
checks rounding/gradual underflow after callbacks and latches refusals. Material
CLI-red detects a new exit-code bug: changed E produces component FAIL but exits
0 because the parent rate status is PASS. Final code uses the selected component
status, and all ten cases pass. Both behavioral failures and the initial missing
header build remain archived. Original negative tests and margins are unchanged.

## Scope and next step

Runtime material reconstruction and its declaration format are version 1.
Original IR/material/profile/3MF and fingerprint schemas are unchanged; no
persisted cache is introduced. Exact.hpp factors existing verifier arithmetic
without importing planner/writer/GCodeProcessor decisions. The CLI's strict
nested registry owns 19 policy fields and eight event fields; source declarations
are inputs, not collision/support certificates. Rounded gap parameters remain a
declared section model and must be checked against actual support independently.

Width/dose intervals and original erosion parameters are retained separately.
Outer boxes are broad-phase bounds only, never filled lower support volumes.
Full lower/upper set membership, union/coverage and continuous head/contact checks
remain required, as do qualified delivered-dose assumptions, transforms/flow,
complete prolog/end/filter/routes and B13 job/plate/software/resource/hash binding.
Work is cumulative and cooperative; hard process containment is still pending.

Configured build label 6b0fc1f9 is the parent, not this implementation. Source,
dependency, four binary and raw/archive hashes bind current evidence. Independent
safety review, current-source Linux, Windows and physical evidence are separate.
Saved CI observation confirms B11 69c4b339 succeeded; parent rates 6b0fc1f9 is in
progress. New material source needs its own run after push. Full B01–B15 stays
IN_PROGRESS and guarded export stays BLOCK. Software continues independently of
firmware-version/material-brand questions.

Implementation commit: `git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B12-final-material.md`.
