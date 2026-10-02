# B12 final-byte continuous run lower coverage

Invariant: ADR-0073. A separate protected synthetic common-run model reconstructs
exact adjacent forward collinear deposition runs from the independent final-byte
material prefix. Every packet keeps its own minimum E-derived dose, gap/top and
section. Whole query inflation crosses internal packet seams with the original
loss/error; pressure, dwell, travel, turns, reversals and section changes split
runs. Per-event erosion retains its original semantics and failures.

## Observed evidence

macOS ARM64, Apple Clang 21.0.0, Release. Final four-target build succeeds in
20.168 seconds. LinearMaterial.cpp keeps -fno-fast-math/-ffp-contract=off without
PCH. The headless executable links the independent verifier library without
slicer/GUI libraries. The configured 6b0fc1f9 label predates this source; exact
source/dependency/binary hashes bind the tested build, with full B13 pending.

The final combined serialization/parser/rate/material/solid/joined selection
executes 33 cases / 2888 assertions in .104 seconds process wall time. Four new
joined cases exercise rectangle/stadium sections, exact rotated forward frames,
reverse frame direction, changed dose/gap, actual partial fronts, empty prefix,
near-collinear turns, direction reversals, section mismatch and dwell/pressure
interruptions. Original per-event negatives stay. The existing union case also
checks protected complete coverage across two run owners, partition bounds,
disjoint interiors, summed partition volume and missing-region/depth/cell refusal.

Separate 113-bit decimal replay verifies complete expanded longitudinal slices,
each original E/flow-derived minimum dose, variable sections and actual run ends.
The oracle checks actual continuity/direction/kinds and accumulated longitudinal
extent; it calls no verifier/planner parser or geometry functions. Stale source/
material/rate/join policy, exhaustion, caller mutation and late positive/negative
numeric publication cannot return partial cover proofs or witnesses.

CTest executes 392/392 with zero failures/skips, gate wall time 96.069 seconds.
Six fresh strict OFF/ZAA G-code/replay pairs pass at unchanged IDs, timestamp
normalization and the established additive OFF-default allowance. Original
normative-package and pinned-source audits pass.

The new joined CLI harness executes 21 cases in .155 seconds; retained cover,
material and rate harnesses execute 18/10/10 cases (.138/.089/.089 seconds).
Strict separate six-field policy and six-field query reject changed/duplicate
keys, versions, models, nesting, IDs, confirmation claims and non-Lower roles.
All 59 cases preserve their input bytes. Selected component exits are 0/2/3;
every report stays job UNKNOWN/export false. Replay, material, prefix, run and
cover share the new mode's original one-second deadline. These cooperative
guards do not provide hard process containment or atomic complete-job binding.

## Complete native final-byte example

The final native evidence execution passes 81896 assertions in 8.947 seconds.
All 2218 original rows (2092 deposits, 126 Travels), exact final bytes and current
rate-policy JSON match the preceding solids checkpoint. Candidate size remains
128890 bytes, SHA256
`d31bc1ef4ec3c4c1342d0ec2ecd89818749acf52a33ce77a1f149d32f32220a8`.
All 2218 material declarations and other material-policy values also match;
the current recaptured source_fingerprint differs and is recorded explicitly.
The new protected current policy supplies the sidecar; no older report is reused.
All seven original head parts, Z=0 tip, .01 mm required margin and original
growth/loss/numeric/dose allocations remain.

The actual prefix is 2217 completed records plus .5 of the last deposition.
It reconstructs 1900 exact runs. A whole box deeper inside the last eight-packet
run has one owner, run 1899, zero-based record indices 2210–2217. Bounds are
[20.021364499999997,19.005571,4.509588498832056] to
[20.021384499999996,19.005591,4.509608498832055]. Independent native oracle and
separate headless process both prove declared joined Lower cover; standalone
work is 122965, one cell/leaf, .396 seconds, exit 0.

The original half-front box remains near the actual eroded current end:
[20.046301249999996,19.005571,4.511258323238683] to
[20.046321249999995,19.005591,4.511278323238682]. Joined Lower returns expected
FAIL, work 122945, .396 seconds, exit 2. Joining cannot repair this external
front. Per-event Lower at the new deeper interior also retains expected FAIL,
work 130214, .415 seconds, exit 2. Absence of guaranteed Lower coverage is not
a physical void. The original complete native B09 exit still returns UNKNOWN
without a route snapshot or witness.

## Retained failure and scope

Joined1 exits 42 on the analytical fixture's 1 ms dwell: it violates the existing
100 Hz event-rate limit before material reconstruction. The fixture becomes
10 ms, without changing the rate policy. Joined2 and every final selected suite
pass. Its earlier counts precede additional oracle/guard assertions. The two
native CLI exit-2 results are expected diagnostic refusals, not failed harnesses.
All raw records are retained separately from final checks.

New joined policy/runtime version is 1. Existing IR/material/profile/3MF/ledger/
fingerprint/default contracts remain unchanged. This explicit common-run model
does not infer independently displaced packet bonding, pressure calibration,
physical material qualification or contact. Curved/zigzag joins, actual support
gaps, complete head/contact/route/cap coverage, union volume, qualified delivery/
transforms and B13 job/plate/resources/software/final-byte/report gate remain
required. Independent safety review, current-source Linux, Windows and physical
qualification are separate. Saved 11:24 UTC CI observation has parent solids
41b03282 pending and material 380967dd in progress; rates 6b0fc1f9 succeeded.
Full B01–B15 remains IN_PROGRESS, export BLOCK. User-declared standard U1 head/
.4 mm nozzle is retained; firmware/material-brand questions do not block software.

Implementation commit: `git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B12-final-joined.md`.
