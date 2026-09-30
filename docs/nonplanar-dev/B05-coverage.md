# B04/B05 continuous coverage from actual deposited material

Invariant: a continuous physical footprint x vertical range is Covered only if
its exact partition is entirely inside the selected deposited-material union.
ADR-0032 defines contract 1. Query results retain prefix, requested domain and
representation; a support consumer explicitly requests the lower representation.

Three new geometric cases / 34 assertions and one actual native case / 16
assertions pass with NoAssertions. The combined existing material suites execute
10 cases/4500 assertions and four native-body cases/46134 assertions. Positives
cover a footprint spanning two overlapping rectangular rows, a partial current
bead and rounded sections. Negatives expose a hole although all four corners are
inside material, reject future/unlaid coverage, and preserve Unknown for limits,
stale/cancelled/late work, reversed domain and uncertain butt boundaries.

The native positive uses every real reserved-body segment and the same declared
0.01 mm inner XY/Z losses as the body material binding. Inherited/conversion/flow
error remains separately charged. The full [17,23]x[17,23] footprint at Z=1.9 is
inside actual D_lower; the same footprint at Z=2.5 is Uncovered with an Outside
witness. Source CAD is never substituted for laid infill.

An independent oracle constructs inscribed rounded-bead cross-section rectangles
at that plane using long-double calculations, shrinks them by 64 microns before
1-micron integer quantization, then subtracts their union from the target through
native Clipper. The difference is empty. This is a second continuous algorithm,
not the planner's membership call or a point grid. On Apple ARM64 long double has
the same precision as double; the explicit oracle inset is much larger than
arithmetic/quantization error in this small fixture. It does not relax production
margins. The exact planner uses outward interval projections/nonlinear sections
and exact rational closed-child partitioning.

Initial tests revealed early depth refusal before an obvious hole. Certified
counterexamples now reject immediately; unresolved boundaries never become
Covered. Current/previous material and state semantics remain unchanged, and
existing canonical ledger/body hash vectors still pass without migration.

Initial native integration hit SIGBUS twice in pinned GNU MP: EPECK lazy rshift,
then eager Gmpq mul_1 under stress at iteration 34. Both crash reports/logs remain.
The linked ARM64 assembler used Apple's reserved x18 counter; the eager wrapper
was not a cure. `GMP-apple-arm64.md` and ADR-0033 record the actual dependency build
correction, unchanged version/checksum and zero-x18 archive audit. After rebuild/
relink, its 197 mathematical programs and 100 randomized-order integration
repetitions with allocator scribbling pass. Exact asynchronous context detail was
not captured; independent review remains pending.

Final corrected Apple Clang 21 macOS ARM64 Release app and both targets build with
exit 0. Selected CTest executes 245/245 without disabled/skipped cases. Six fresh
corrected-library OFF/ZAA G-code/modal comparisons match pinned stock, allowing
only existing normalization and additive nptop_mode=off. Shared final build/test/
differential evidence is referenced under `evidence/GMP-apple-arm64/`; coverage
XML, failures, crash reports, command metadata and source hashes are under
`evidence/B05-coverage/`. No original specification, source fixture or golden changed.

```sh
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialModel],[MaterialCoverage]' --warn NoAssertions
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[BodyMaterial],[NativeCoverage]' --warn NoAssertions
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/GMP-apple-arm64/ctest-final --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label GMP-apple-arm64-final-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/GMP-apple-arm64-final-baselines --allow-nptop-off-default
```

Author review checked affine extrema versus nonlinear sections, exact subset/union
partition, candidate filtering, rounded-witness containment, depth/work failure,
callback ownership and explicit result domain/tag. This is not independent safety
review. New-revision Linux/Windows remain NOT_VERIFIED/NOT_RUN; physical is NOT_RUN.
The flat interior footprint proof does not cover the full cap edge/seam, all native
hole/step geometries, curved surfaces, local contact, tool CCD, job binding or
hard worker containment. B04/B05 full scope remains open. Next use actual lower
coverage with upper material/gap bounds in the stepped first-pass solver, preserving
nominal volume and seam obligations. Full B01–B15 stays active; guarded export blocks.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B05-coverage.md`.
Diff: `git show <resolved-implementation-revision>`.
