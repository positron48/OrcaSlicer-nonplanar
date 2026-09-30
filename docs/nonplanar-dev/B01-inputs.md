# B01 exact pre-apply native inputs and revision

Invariant: a guarded Print owns the raw native input before normalization.
Any changed source fingerprint invalidates psGCodeExport and advances a monotonic
revision, including changes hidden by native approximate config/transform diffs.
Identical inputs preserve the revision; failed captures discard the prior source
and advance again, OFF clears guarded inputs and clear invalidates retained state.
All guarded processing/export still blocks after configuration preflight.

ADR-0026 defines the owned graph, limits, source geometry/config identities and
Print settings schema 2. It deliberately replaces the previous expectation that
an unchanged effective Print fingerprint also represents unchanged source input.
The schema-1 fixture and earlier evidence remain historical; the new independent
schema-2 vector binds revision and source fingerprint.

Six native cases/90 assertions cover exact binary32 mesh/SHA vectors and input
JSON, adjacent float/double inputs, immutable source meshes/configs/transforms/
painting, raw material and layer-range controls, unit/offset/plate-action data,
source replacement and export invalidation, stable successful reapply, OFF,
clear and aggregate graph/config/annotation failures. All 54 B01 cases/4591
assertions pass with NoAssertions. The initial test-first build fails because
InputSnapshot does not exist. The first implementation build finds two test API
mistakes (native triangle index alias and matrix comparison); their diagnostic
excerpts and exit codes are retained. Corrected final app and native targets
build successfully with Apple Clang 21 on macOS ARM64.

Selected CTest discovers and executes 221/221, with zero disabled/skipped cases.
Six fresh OFF/ZAA G-code/modal replay pairs match pinned stock, using only the
existing timestamp normalization and additive nptop_mode=off allowance. Config
schema-1 oracle remains unchanged. Package/checkout audit passed their documented
source/documentation-only scope; these are not runtime qualification.

Evidence, exact commands/exit codes, final XML and source/binary hashes are in
[evidence/B01-inputs](evidence/B01-inputs/source-manifest.json). Raw build/test output
is retained under build/nonplanar-evidence/B01-inputs. Source/documentation
whitespace checks pass; generated native CTest/JUnit evidence is retained bytewise.
Representative commands:

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[InputSnapshot]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[B01]' --warn NoAssertions
python3 scripts/nonplanar/input_fingerprint_oracle.py
python3 scripts/nonplanar/print_config_fingerprint_oracle.py
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B01-inputs/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B01-inputs-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B01-inputs-baselines --allow-nptop-off-default
```

The source scope is the owned Nonplanar input encoder and native Print integration.
The canonical config encoder is extracted without changing config schema-1 bytes.
Author review is retained separately; independent safety review remains pending.
Current Linux run 36758101513 at prior commit 7df1d765 remains in_progress in the
saved API snapshot, so no success or new-revision Linux result is claimed. Windows
and physical execution remain NOT_RUN. The user reports standard U1 head and
nominal 0.4 mm nozzle in B15-u1-declared-setup.md; this is not measured qualification.

The full B01–B15 objective remains active; gate-b-plan.md records every normative
acceptance dependency. Complete compatibility registry, native GUI plate/token
invalidation, original file/import error binding, whole-job/software/profile/scene
ownership, partition/material/planning/replay/publication and B15 remain open.
This source token alone is not a complete job identity or a stale-callback gate:
it is scoped to one Print lifetime and downstream jobs must bind their own IDs
and all remaining dependencies. Hard parser/worker resource containment is not
established by synchronous snapshot limits.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B01-inputs.md`.
Diff: `git show <resolved-implementation-revision>`.
