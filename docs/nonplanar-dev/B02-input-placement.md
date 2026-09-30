# B02 owned native source to STL/plate placement

Invariant: downstream placement consumes the pre-apply owned native mesh and
transforms instead of rebuilding them from mutable GUI state. The result binds
NativeInputSnapshot, captured original STL bytes, plate frame and revision. An
input/plate mismatch or failed computation returns no accepted payload.
ADR-0027 defines the internal adapter and remaining job/plate boundaries.

Two new native cases/62 assertions pass with NoAssertions. Positive geometry
matches a separately computed native Model mesh after nonuniform scaling,
rotation and plate translation; the source Model is then deleted and the
caller's input binding cleared by a callback. The retained source/config identity
and STL pointer remain unchanged; the actual placed mesh feeds a useful nominal
upper projection. Negatives reject unrelated STL, selected-plate mismatch,
missing/zero revision, unprintable instances, cancellation, stale callbacks,
deadline expiry and callback exceptions without snapshots/geometry/error payload.

All 46 B02 cases/791 assertions pass with NoAssertions. Apple Clang 21 macOS ARM64
Release application and both native targets build with exit 0. Selected CTest
executes 223/223 with zero disabled/skipped cases. Six fresh OFF/ZAA G-code/modal
replay comparisons match pinned stock, with only existing timestamp normalization
and additive nptop_mode=off allowance. No golden/spec/source fixture was changed.
The test-first build fails because the new placement API does not exist; error
excerpts and command/exit metadata are retained. Source whitespace checks pass;
native generated CTest/JUnit evidence remains bytewise, including stdout spacing.

Commands/exit codes, XML, discovery, differential manifests and source/binary
hashes are in evidence/B02-input-placement/source-manifest.json. Raw output is
under build/nonplanar-evidence/B02-input-placement. Representative commands:

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[InputPlacement]' --warn NoAssertions
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[B02]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B02-input-placement/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B02-input-placement-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B02-input-placement-baselines --allow-nptop-off-default
```

The implementation reuses the native Model/mesh and existing centering/placement
chain. Config values remain owned for job provenance; this private reconstruction
is only a geometry view and cannot be used as a resolved Print config. The source
scope is MeshPlacement.hpp/.cpp and targeted native import/placement tests.
Independent safety review, new-revision Linux and Windows runs remain pending;
physical execution is NOT_RUN. Native GUI plate membership/origin invalidation,
file-to-job association, worker containment and complete JobSnapshot remain open.
Full B01–B15 objective remains active, per gate-b-plan.md. Body/cap partition and
actual whole-body semantic/material/planning integration are the next dependent
software tasks; no guarded export approval was added.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B02-input-placement.md`.
Diff: `git show <resolved-implementation-revision>`.
