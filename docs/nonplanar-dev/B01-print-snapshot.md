# B01 owned native Print settings aggregate

Invariant: a captured native Print preflight retains its full settings, effective
PrintConfig (including filament overrides), all resolved object/region settings,
counts, plate index/origin and source diagnostics in one immutable aggregate.
Later source modification/clearing cannot change its settings or fingerprint.
ADR-0025 defines the settings-only boundary and canonical schema 1. The actual
Print block consumes the capture; guarded slicing/export remains blocked.

Seven native cases cover independent Python canonical/SHA-256 vectors, two real
native regions with object/volume inheritance, OFF overrides, exact plate bits,
adjacent double values, multiple instances, retained input/model conflicts,
invalid plate state, the OFF bypass, aggregate region/byte limits and actual
filament retraction resolution. The native full config retains 0.8 mm retraction
while PrintConfig and both captured effective/region values use 1.25 mm; a later
1.5 mm override changes identity without altering the earlier capture.

Five initial cases fail against empty capture/encoder implementations (eight
failed assertions, exit 42). One initial test incorrectly expected a one-ULP
incoming value to survive repeated Print::apply. Native ConfigOptionFloat uses
approximate equality, and PrintApply updates full settings only on a native
diff. The failed log/XML is retained. The corrected test proves the cached value
stays unchanged, while a fresh Print retains the neighboring full-config value
and receives a distinct identity. This corrects the test's source-state premise;
it does not weaken the exact captured-state identity. The first test build also
used two nonexistent convenience APIs; corrected native erase/set calls and
the failed build metadata are retained.

macOS ARM64 Release app and both selected native test targets build with exit 0,
using Apple Clang 21.0.0. Seven cases/82 assertions and all 48 B01 cases/4501
assertions pass with NoAssertions. Selected CTest discovers and executes 215/215
without disabled/skipped cases. All six fresh OFF/ZAA G-code/modal replay pairs
match pinned stock under the existing timestamp normalization and additive
nptop_mode=off allowance. No golden or schema/default migration was made.

Exact commands/exit codes, red/final native XML, CTest discovery/results, baseline
manifests, independent vectors, author review and source/binary hashes are in
evidence/B01-print-snapshot. Raw build/capture output is retained under
build/nonplanar-evidence/B01-print-snapshot*. Representative final commands:

The source/documentation whitespace check passes. A whole-commit whitespace
check reports trailing spaces in native CTest JSON and test stdout inside JUnit;
those generated evidence bytes are preserved rather than reformatted.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[PrintSnapshot]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[B01]' --warn NoAssertions
python3 scripts/nonplanar/print_config_fingerprint_oracle.py
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B01-print-snapshot/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B01-print-snapshot-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B01-print-snapshot-baselines --allow-nptop-off-default
```

The source scope is Policy.hpp/.cpp, Print.hpp/.cpp and targeted native tests;
the independent Python oracle is an evidence helper, not a production engine.
Author review checks owned storage, native resolution precedence, exact framing,
aggregate limits, OFF bypass and all-block behavior. Independent review remains
pending. Input values discarded by native approximate comparison/normalization,
source geometry/transforms, GUI plate membership, revision/worker binding,
hard resource containment and a complete JobSnapshot remain open. This is no
tool/scene/firmware qualification, hybrid planning, permission to print, or Gate
A/B closure. Capture source inputs before native apply as the next dependent step.

Separately refreshed Linux evidence: run 36454696019 at d57e9325 succeeded for
native build, selected CTest, STL CLI and application build. Its live public
run/job/step records are preserved here. Dependencies came from the exact cache;
archived JUnit/compiler files were not downloaded. New milestone Linux execution
and Windows execution remain pending; physical qualification remains NOT_RUN.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B01-print-snapshot.md`.
Diff: `git show <resolved-implementation-revision>`.

Follow-up 2026-09-30: `B01-inputs.md` and ADR-0026 now capture raw source inputs
before native normalization and bind Print settings schema 2 to a source
fingerprint/revision. The schema-1 statements/counts above describe this earlier
commit. Whole-job/plate/profile/scene binding and export remain pending.
