# B04 actual derived partition index stability

Bounded representation fix implemented; full B01–B15 remains IN_PROGRESS.
ADR-0087 defines the scope; separate author audit is recorded in
`B04-partition-order-review.md`. Independent safety review remains pending.

Native CGAL output descriptor order changed body vertex16/17 and dependent
fingerprints across equivalent runs. Conversion now constructs the actual body
and cap vertex arrays in exact lexicographic XYZ order before the existing float
conversion. It remaps all triangles, cyclically rotates each to its smallest
index without reversing winding, then sorts oriented faces lexicographically.
There is no retriangulation, removal, coordinate change, hash normalization or
equivalence-based certificate reuse. Original/reservation/source files and
production formats remain unchanged. Old snapshots keep their actual hashes.

All sort comparisons participate in the existing root stop function, retaining
deadline/cancellation/staleness/fenv guards and original size/error limits.
Exact shared point conversion and interface triangulation, original body/cap
volume and complete post-conversion topology audit remain. A failed comparison
cannot publish a partition. Consumers receive the newly constructed indexed
meshes and their freshly rebuilt dependent owners.

The expected-red property test fails on the former ordering. Its positive now
checks six descriptor/cyclic-start permutations of the same oriented affine
input, exact actual body/cap indices, volume, error and shared-interface counts
plus unchanged caller meshes. Existing independent barycentric ray/volume and
hole/stepped/interface, resource/error/cancel/stale cases also pass.

Fresh separate native processes produce byte-identical33 final component files,
including all actual material source fingerprints; their independent final-text
geometry/support references run again. Compared with the preceding milestone,
39 of51 native files are byte-exact. Twelve reflect actual new body/cap indices
and dependent hash leaves. An independent oriented-coordinate multiset check
proves every original body22 vertices/40 triangles and cap14/24 triangle remains;
original and reservation8/12 arrays/properties are exact. This comparison
documents geometry preservation; it never substitutes for current proof owners.

Candidate is unchanged:2205 records/128014 bytes,
SHA256 `b8eb15681127c1a8db57ce0aa257326763f16832b82fdafd13ed091c4bd98ef8`.
Every non-material final Travel/rigid Deposit/contact v1/contact v2/combined
support input, proof and refusal remains byte-identical. Combined native4-packet
PASS retains48 cells and637876 API work; fresh CLI598131 work/0.528 s under the
original1s root. Full108-packet contour older-side FAIL and earlier unproved cell
remain; a stable source representation does not resolve that contact conflict.
Actual fresh body source fingerprint is
`6c1b28d7a5ab20102811a8fe3516c60f5724096726be7837a6316e025ded02b4`.

macOS26.5.1 ARM64 / Apple Clang21.0.0 Release: all four-target build exit0;
partition6/6319 and final-byte30/2433417 assertions, selected CTest450/450,
zero failures/skips,108.150 s gate. All437 CLI cases retain expected statuses
and unchanged inputs. 434 prior analytical outputs are byte-exact; three nominal
UNKNOWN/CANCELLED cases differ only in work/cell counters at the original1s
deadline. Six fresh strict OFF/ZAA pairs, package/source audit, workflow YAML,
strict no-PCH/-fno-fast-math/-ffp-contract=off compilation for the changed exact
partition and verifier, independent context/artifact/report oracles pass.

The initial test-only namespace/type build errors and expected-red ordering
failure are retained in the lossless archive. Exact source/dependency/binary
hashes and commands are bound by `evidence/B04-partition-order/source-manifest.json`.
Linux new source awaits its own run; Windows/full GUI/physical tests are NOT_RUN.
Normative documents and real presets/printer/cloud remain unchanged. Fixed17
stays4 PASS/RUN +13 UNKNOWN/NOT_RUN; full B remains active and export BLOCK.

This fixes descriptor-order stability for an identical actual triangulation.
Universal CGAL triangulation/cross-platform float determinism, full software/
resource/source-to-plan/GUI provenance and verified publication remain open,
alongside qualified contact, seams/full fixed-width cap fill, complete job
geometry/order/transforms/delivery and B14/B15.
