# B10 full journal motion limits

Invariant: ADR-0068. Every original source record receives a protected owned
linear motion step. Retain all XYZ, deposited amounts, bead/pressure metadata,
IDs and order; reduce only necessary XYZ speed/acceleration ceilings. Cartesian
axes and Cartesian/CoreXY drives, E, commanded-equivalent Q/cross-section,
retraction length and event frequency participate. Nominal volume has no second
3D/XY correction; E applies diameter area and flow once.

The declared model stops at each event. Rest-to-rest timing and explicit instant
Travel dwell are output obligations; unsupported firmware lookahead refuses.
Pressure uses its own filament-coordinate schedule without changing IR XYZ speed
semantics or depositing phantom material. A future exporter must enforce full
stops, assigned acceleration and dwell, and the final-byte verifier must replay
them. This certificate alone cannot qualify ordinary blended G1 output.

## Observed software result

macOS ARM64 / Apple Clang 21.0.0: final sequential build4 succeeds for application
and both native consumers in Release, 17.423 seconds. New MotionPlan.cpp has
-fno-fast-math and -ffp-contract=off without PCH; MSVC registration uses /fp:strict.
Four analytical cases / 238 assertions pass. Material suite: 122 / 48037,
39.523 seconds. Native body/cap suite: 14 / 134452, 36.661 seconds. Focused complete
native case: 1 / 26564, 9.125 seconds (9.062 inside the test). No failures/skips.
Final selected CTest executes 367/367 with no failures/skips in 91.411 seconds
(gate wall time). Six fresh strict OFF/ZAA pairs
pass with original normalization/object IDs and only the established additive
OFF-default allowance. Original package and checkout audits pass separately.

The complete original native journal has 2218 body/cap/later records here. All
receive steps under one five-second policy and 28885 work; declared duration
upper is approximately 1383.7 seconds. All canonical rows match an owned copy
changing only the admitted ceilings, and an independent long-double nominal
dose sum lies in the total interval. Record counts are local observations, not
platform literals or a physical-time prediction. Its B09 exit remains UNKNOWN.

Independent 113-bit formulas check every analytical axis/drive rate and
acceleration, E/Q/cross-section, triangular ceiling, whole trapezoid duration,
nominal dose and filament totals. CoreXY diagonal drive coupling halves the
admitted feed relative to Cartesian drives; a 0.0001 mm Travel slows below
0.021 mm/s for event frequency. Lower Q slows the complete original source.
Diameter/k tests detect wrong area scaling or double flow. Original pressure
amount/state and instant Travel retain exact canonical records.

Missing source, unsupported version/model/kinematics/confirmation, invalid limits,
out-of-domain XYZ, cross-section, pressure length, work/records/deadline, stale
source/scene/policy, cancellation, caller mutation, final publication and rounding
refuse without a complete snapshot. Original positive/negative geometry tests
and native full-ledger B09 boundary refusal remain unchanged.

## Retained build failures

red-build exposes the missing header. A repeated build2 was mistakenly started
before build1 completed, then interrupted. It left four overlapping library
objects missing; build1 fails at archive creation. Both logs and interruption
exit 130 are kept. No test is run on those builds. Once both handles are terminal,
sequential build3 recovers missing objects and completes the application; build4
forces the final MotionPlan and both edited test objects, without concurrent
builders. Only build4 supplies final test evidence. The CMake regeneration also
updates the configured Git label from 31461d50 to parent 89f009b9; the larger
rebuild is retained, not misreported as a small incremental compile.

## Provenance and remaining scope

Runtime linear-motion-plan contract is 1. Exact motion-policy identity covers all
fields. Existing material/scene/IR/3MF/profile schemas and fingerprint formulae
remain; changed ceilings change the ordinary ledger hash and invalidate old
motion/job proofs. Full copied ledger and scene preparations, source walks,
axis/drive checks and publication share one work/deadline and source/scene/policy
revisions. No persisted cache, profile edits or production export route is added.

The archive binds source/dependency/binary hashes, exact commands/exits, JUnit IDs,
strict compile command, baseline bytes and raw-byte mappings. Configured label
89f009b9 identifies the parent, not the new uncommitted implementation; hashes bind
the actual build. Full B13 software/resource provenance remains pending.

Full cap/contact/tool access/order, actual firmware/transform and flow calibration,
stop/acceleration/dwell enforcement, every complete job motion and independent
final rounded-byte replay remain required. Current-source Linux, Windows,
independent review and physical qualification are separate. The saved 05:05 UTC
Linux observation has B09 89f009b9 / 36965840048 in progress; inherited 4903be4a /
36961569541 fails the already corrected macOS-specific body-count literal, with
original annotations retained. No timeout or geometry policy was relaxed.
Guarded export stays BLOCK; full B01-B15 stays IN_PROGRESS.

Implementation commit: `git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B10-motion-plan.md`.
