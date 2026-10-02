# B12 final-byte solids and continuous region coverage

Invariant: ADR-0072. The independent final-byte verifier now classifies complete
closed 3D boxes against nominal, upper and lower constant-flux solids. Protected
union-cover partitions retain actual source/event owners. Neither filled outer
AABBs nor sampled query vertices supply positive coverage evidence.

## Observed evidence

macOS ARM64, Apple Clang 21.0.0, Release. All four targets build; final build7 is
3.779 seconds. Strict LinearMaterial.cpp flags are -fno-fast-math/-ffp-contract=off
without PCH. Headless link contains only the independent verifier library, with
Boost/JSON headers and no slicer/GUI library. Configured label 6b0fc1f9 is older
than this source; exact source/dependency/binary hashes bind the tested build.

The final combined serialization/parser/rate/material/solid selection executes
29 cases / 1940 assertions in .085 seconds, including five new solid cases.
Independent 113-bit formulas bound whole rectangle/rounded boxes, dose in opposite
directions, and every union leaf. Leaf bounds, disjoint interiors and total
volume check the complete finite partition independently. Rotated bounding
corners, rounded side/top corners, short eroded butts, unlaid/future material,
stale source/rate/material policy, work/cell limits, caller mutation and late
numeric/cancellation guards refuse as appropriate. Positive unions span distinct
bead owners; they do not merely test one point or the four query corners.

CTest executes 388/388 with zero failures/skips, 95.301 seconds gate wall time.
It includes the complete existing native/material/body/collision suites and all
new cases. All six fresh OFF/ZAA G-code/replay comparisons pass at unchanged
normalization/object IDs and the established additive OFF-default allowance.
Original normative-package and pinned-source audits pass.

Headless cover harness: 18 cases, .471 seconds, all expected outcomes. Existing
material and rate harnesses: ten cases each, .426/.421 seconds. Selected
component exits remain 0/2/3; every diagnostic retains job UNKNOWN/export false.
Cover metadata rejects wrong/duplicate keys, representations, versions, negative/
fractional/oversized counts, extra axes and excessive nesting. Future/empty-prefix
and bounding-corner coverage returns FAIL with an uncovered region. Parsing,
rate/material/prefix/cover stages share the new mode's original one-second
absolute deadline. Cooperative limits remain distinct from hard process isolation.

## Complete native final-byte example

Final native evidence execution passes 75158 assertions in 8.772 seconds. All
2218 rows (2092 deposits, 126 Travels), original source/model losses/growth/error,
all seven head parts, Z=0 tip and .01 mm required margin remain. Final bytes are
still 128890 bytes with SHA256
`d31bc1ef4ec3c4c1342d0ec2ecd89818749acf52a33ce77a1f149d32f32220a8`.
Current bytes/material declarations match the preceding checkpoint exactly.
Current rate-policy JSON is emitted from the actual protected rate snapshot;
its values agree with the preceding fixture and its initial pose with the sidecar.

The actual last deposition's half-progress prefix contains a whole positive
volume box near its laid midpoint. Nominal coverage has one leaf owned by event
2218. The separate process replays those same complete bytes/current policies/
query and returns nominal component PASS, work 130924, .405 seconds. Lower cover
returns the expected diagnostic FAIL (exit 2), work 130214, .398 seconds, with the
entire requested box outside the accepted per-event lower solids. Bounds are
[20.046301249999996,19.005571,4.511258323238683] to
[20.046321249999995,19.005591,4.511278323238682]. This proves missing guaranteed
coverage in the declared event-erosion model, not a physical print void.

The .014249 mm last full XY packet and half laid length are shorter than twice
the unchanged .014362 mm XY loss/error allocation. Its lower butt interval is
empty. Nominal occupancy cannot establish lower support. Whole joined packet
solids need their own independently reconstructed continuity/uncertainty proof;
old pre-rounding packet/run certificates are not reused. The original complete
B09 exit still returns UNKNOWN without route snapshot or witness.

## Retained failures and scope

Scaffold-build exits 1 on the missing new symbols before implementation (no test
run). Solids1 exits 42 because depth-first traversal reaches a boundary UNKNOWN
before an available missing-region witness. Breadth-first traversal fixes that
case without declaring uncertain boundaries empty or safe. Cover-cli-red exits
1 when the prior material report overwrites the new coverage component/reason;
final reporting uses the actual selected component. Native1 exits 42 only because
the optional evidence directory was not created; its geometry assertions pass
before that setup failure. Corrected native2/final runs are complete. The final
native lower CLI's exit 2 is an expected geometric refusal, not a failed test.
All historical raw output is retained separately from final checks.

Runtime region-query metadata is version 1. Original IR/material/profile/3MF/
fingerprint schemas and defaults remain unchanged. Coverage is of the selected
solid representation over the requested region, not a calibrated material union
volume, support-gap qualification, contact/whole-route proof or export approval.
Continuous packet joins, actual support gaps, head/contact/full cap coverage,
qualified dose/transform assumptions and complete B13 job/plate/software/resources/
final-byte/report binding remain required. Independent safety review, current-source
Linux, Windows and physical qualification remain separate. Saved 10:37 UTC CI
observation confirms parent rates 6b0fc1f9 succeeded; material 380967dd is in
progress. New solids need their own run after publication. Full B01–B15 stays
IN_PROGRESS and guarded export stays BLOCK.

Implementation commit: `git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B12-final-solids.md`.
