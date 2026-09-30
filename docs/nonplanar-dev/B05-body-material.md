# B05 actual native body material binding

Invariant: all supported native body segments become owned declared material with
source region/path/segment associations and an explicit plate-to-physical
translation. ADR-0031 fixes the reconciliation and binding contract, schema 1.
The original body retains native width/height/rate, geometry and settings.

Three native cases / 46118 assertions pass with NoAssertions. The actual dense
reserved body from B04 supplies every segment; independent long-double calculations
check original rate times physical XY length and effective rounded-section width.
The summed commanded amount agrees with native body volume within 1e-6 mm3.
A prefix point behind the advancing event has guaranteed inner material, while
the reserved cap interior remains outside the complete upper representation.
Callbacks destroy the caller's body handle without losing source or revision.

Native Flow rounds area to float; exact native width/height geometry has a slightly
different area. The adapter keeps commanded flow, reconstructs effective width,
and records bounded width/volume reconciliation. It checks the actual native
Flow function, without arbitrary flow tolerance or a second ratio. Per-segment
reconciliation is below 1e-6 mm and 1e-8 mm3 in this fixture. Partition, native
origin/path, translation arithmetic and half-width correction add to a distinct
numerical allowance, which must remain <=0.05 mm. Physical inner/outer assumptions
are separate declared inputs, without operator qualification.

Source scaled coordinates must exactly reproduce retained physical path points
through the named NativeScale and native region origin/layer Z. Integer bounds
are checked before subtraction. Negative cases reject 1% flow mutation, bridge
role, shifted or extreme scaled point, missing context, exhausted error/record
budget, out-of-domain translation, cancelled/stale/late callbacks and changed
rounding. Existing unsupported-role negatives remain unchanged. A struct/hashlib
vector independently checks body/material parent IDs, translation and the complete
association chain; null/synthetic vector parents are byte data, never accepted
factory inputs.

Retained enumeration plus explicit straight connectors is a diagnostic material
sequence. It has not qualified native G-code order, entry/exit, travel, speed,
acceleration, firmware or contact references. It creates no fictitious bead during
Travel and supplies no retract state beyond the declared initial Ready state.
No executable plan or export route is provided. B09/B10 must qualify the final
order and limits before this becomes a printer job.

Apple Clang 21 macOS ARM64 Release app and both native targets build with exit 0.
Selected CTest discovers and executes 241/241 with no disabled/skipped cases.
Six fresh OFF/ZAA G-code/modal comparisons match pinned stock with only existing
normalization and additive nptop_mode=off. No original bundle, source fixture or
golden changed. Evidence metadata/XML/discovery/differential/hash vectors and
source/binary hashes are in `evidence/B05-body-material/`; raw logs are in
`build/nonplanar-evidence/B05-body-material/`. The initial syntax-only test-first
failure includes absent API and a const-record test assignment; the latter was
fixed by constructing a new immutable snapshot.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[BodyMaterial]' --warn NoAssertions
python3 scripts/nonplanar/body_material_fingerprint_oracle.py
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B05-body-material/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B05-body-material-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B05-body-material-baselines --allow-nptop-off-default
```

Author review checked all-segment mapping, coordinate conversion, float flow,
aggregate budget, callback ownership, prefix state, source identity and integer
arithmetic. It added source-point consistency and explicit role checks. Required
independent critical review is pending; Linux new revision is unverified, Windows
and physical execution are NOT_RUN. Source hashes identify the built changes;
CMake's existing compiled commit macro remains 31461d50 until reconfiguration.

Full B05 contact/CCD and B04 dense whole-footprint coverage/refusal remain open,
as do seam, native hole/step domains and actual job/worker integration. Next prove
coverage over a continuous footprint from the union of actual inner beads. A point
grid or the parent solid CAD cannot establish it. Full B01–B15 stays active and
public guarded export remains blocked.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B05-body-material.md`.
Diff: `git show <resolved-implementation-revision>`.
