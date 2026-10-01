# B06 source-bound affine pass selection and prospective cell quotas

Invariant: retain the original final affine surface, choose exactly the requested
number of parallel prospective passes within actual first-pass and later
vertical/normal thickness bounds, and allocate whole-cell nominal quotas only
within an explicit total volume error. ADR-0036 defines internal contract 1.

Three new geometric cases / 54 assertions pass. A rounded-shoulder circular
antiderivative independently confirms the total nominal volume after selecting
four passes. A slope confirms distinct vertical/normal thickness without a
second slope-volume correction. Infeasible pass counts, excessive first gap,
missing support, stale or exhausted work/precision, callback mutation and excess
numerical error cannot publish a stack. The final target vertices are retained.

One actual native case / 74 assertions passes. Derive the target from the owned
original flat-block mesh, after native body reservation beginning at Z=3.2 and
reconstruction of all actual Orca beads. The same 6x6 mm interior and 0.01 mm
inner/outer material assumptions are retained. With four illustrative passes,
the first prospective surface is Z=3.37000064498729035, final Z=4, and the actual
first gap interval is [0.14996744761757827,0.27003255238242035] mm. The total nominal
cell volume lies in [28.97352050100678156,28.98351843215683843] mm3; allocation error
is bounded by 0.00499896557503021 mm3 against an explicit 0.01 mm3 allowance.
These are software fixture values, not U1 settings or delivered extrusion.

The wrapper checks parent fingerprints, exact affine source membership and
source/frame error, and executes 6240 exact reservation half-space tests. Only
convex reservations are supported. A translated physical origin (100,200,10)
preserves the source target at physical Z=14 and overlapping volume bounds;
untranslated coordinates reject. Bad parent identity, invalid patch, insufficient
passes, outside source/cap domain, exhausted reservation work, stale/cancelled/
late work and a deeper reserve that cannot reach the target with four passes all
reject. Resetting the caller's parent during callbacks does not lose owned data.

Test-first syntax failures for the missing core and native APIs are retained
(exit 1). The native integration first returned INVALID_FOOTPRINT_QUERY because
the existing capsule API requires positive radius. Supplying the already-budgeted
source-error inset as that radius preserves its extent and API contract; no
margin or precision was reduced. That exit-42 diagnostic and the corrected
positive/negative run are archived. One intermediate compile error is retained.

The app and both targets build with exit 0 on Apple Clang 21 / macOS 26.5.1 ARM64 /
Release. Combined geometric suites pass 20 cases / 4646 assertions, actual-body
suites seven cases / 54493 assertions with NoAssertions. Selected CTest executes
258/258 without skips. Six fresh OFF/ZAA G-code/modal comparisons match pinned
stock under the existing normalization and additive nptop_mode=off. The original
package and source checkout audit pass. Source/binary hashes, commands, exit
codes, XML and diagnostics are under `evidence/B06-pass-stack/`; full raw logs
remain under `build/nonplanar-evidence/`.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialModel],[MaterialCoverage],[MaterialTransition],[MaterialIntegral],[PassStack]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[BodyMaterial],[NativeCoverage],[NativeTransition],[NativeIntegral],[NativePassStack]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B06-pass-stack/ctest-2 --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B06-pass-stack-baselines-2
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B06-pass-stack-baselines-2 --allow-nptop-off-default
```

Author review also tightened the source XY round-trip error to an L1 sum over
both coordinate bounds, enclosing the Euclidean inset before final revalidation.
Author review covers preserved source/target, support/gap prerequisites, offset
range, actual plane normals, interval volume sums, total allocation/numerical
error, convex whole-cell containment, frame conversion, ownership and limits.
Independent safety review is pending. Current Linux revision is NOT_VERIFIED;
Windows and physical execution are NOT_RUN. Standard U1 head and 0.4 mm opening
remain user-declared and unmeasured. Firmware/filament names are not needed for
this software implementation.

Full B06 remains open: prospective surfaces do not prove subsequent actual cap
bead support, finite path width, the volumetric seam or contact/head clearance.
Next subdivide step/edge domains and allocate paths with seam corrections before
the complete order/CCD/replay/export pipeline. Full B01–B15 remains active.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B06-pass-stack.md`.
Diff: `git show <resolved-implementation-revision>`.
