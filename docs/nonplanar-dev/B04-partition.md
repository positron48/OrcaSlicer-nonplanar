# B04 owned exact body/cap partition

Invariant: one exact native CGAL corefinement produces body=M minus reservation
and cap=M intersect reservation, with a common interface. The owned source STL,
native inputs, plate frame and revision remain unchanged. This is nominal geometry
for subsequent native body planning, not tool/material/export approval.
ADR-0028 defines the bounded contract and fingerprint schema 1.

Five new native cases/4881 assertions pass with NoAssertions. Independent analytic
volumes and strict-interior barycentric ray samples check an interior reservation
in an affine roof, a through hole and a stepped reserved interface. Tests verify
surrounding walls, disjoint body/cap occupancy, source preservation, shared
interface faces, nonzero measured conversion error and immutable identity after
reservation changes. A Python struct/hashlib oracle independently checks canonical
fingerprint framing. Its synthetic empty-mesh vector is serialization data only.
Negatives reject open/empty/full reservations, missing provenance, cancellation,
stale work, deadline and resource/error-budget violations without a snapshot.

The exact backend uses the existing CGAL dependency with exact constructions;
stock MeshBoolean behavior remains unchanged. Output coordinates use a shared
conversion map and carry outward conversion/inherited-placement bounds. Native
outputs are audited after conversion. Exact and native volume intervals are
recorded separately; volume identity alone does not prove spatial partition.

Apple Clang 21 macOS ARM64 Release application and both test targets build with
exit 0. Selected CTest executes 228/228 without disabled/skipped cases. Six fresh
OFF/ZAA G-code/modal comparisons match pinned stock with the existing timestamp
normalization and additive nptop_mode=off allowance. The original specification,
source fixtures and golden output remain unchanged.

The test-first build fails on the absent new API. Failed Catch2 assertion syntax
and a fixture with stale native bounding-box statistics are retained; the fixture
was reconstructed from its edited indexed geometry before native placement. That
failure was SOURCE_CENTER_OFFSET_MISMATCH before partition, not relaxed geometry
acceptance. One slow --parallel 2 build was deliberately interrupted after the
exact backend compiled and resumed with --parallel 8; exit 130 remains recorded.
The final build and native run pass. Commands/exit codes, XML, CTest discovery,
baseline manifests and source/binary hashes are under evidence/B04-partition/;
raw logs are under build/nonplanar-evidence/B04-partition/.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[VolumePartition]' --warn NoAssertions
python3 scripts/nonplanar/partition_fingerprint_oracle.py
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B04-partition/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B04-partition-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B04-partition-baselines --allow-nptop-off-default
```

Author review: input/output ownership, exact-to-native conversion, common face
identity, fail-closed payloads and cancellation/error budgets were checked against
source and analytic fixtures. This is not independent critical review. Exact
corefinement is opaque between cooperative checks: hard deadline/RSS containment
remains pending. Current input face limit is 5000, not the full 200k target.

The source scope is the isolated VolumePartition modules, their CMake registration,
targeted import tests and independent identity vector. Full B04 still needs native
whole-body trajectories, actual dense under-cap coverage or refusal and volumetric
seam/material proof. B03 upper selection and tool access remain separate. Guarded
slicing/export stays blocked. New-revision Linux, Windows and independent review
remain pending; physical execution is NOT_RUN. Complete B01-B15 remains active.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B04-partition.md`.
Diff: `git show <resolved-implementation-revision>`.
