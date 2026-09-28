# B02 captured native volume centering

Invariant: a captured ModelVolume must correspond exactly to the checked source
STL after the pinned native centering operation, and its additional binary32
rounding must be charged to the source error budget. ADR-0014 defines this local
coordinate boundary and explicitly excludes subsequent model/plate transforms.

Four native cases exercise a translated binary STL block with an independent
Python Fraction oracle (147/268435456 mm maximum L1 error), source allowance
propagation, unchanged source data, half-budget refusal, altered geometry/offsets,
unit flags, modifier role, reload transform and invalid indices. They also cover
owned input capture under callback mutations, cancellation, stale revision,
unsupported arithmetic, invalid limits and missing provenance. The actual native
load_stl -> Model::add_object -> ModelVolume centering route passes separately.

The initial positive tests failed (exit 42, two failed assertions) before the
implementation. The first native Model link required the same NanoSVG
implementation units used by the pinned upstream FFF tests; these were added to
one nonplanar test translation unit. No mock SVG symbols or CMake changes were
introduced. The original linker failure is retained as evidence.

Final macOS ARM64 Release application and test targets build with exit 0. Four
centering cases pass with 70 assertions and NoAssertions enabled. Fresh selected
CTest executes 149/149 without skips/disabled cases. Six fresh OFF/ZAA captures
match stock motion and G-code, with only the existing timestamp normalization and
explicit nptop_mode=off setting addition. Commands, exits, failed/final XML, oracle,
source/binary hashes and baseline manifests are under evidence/B02-centering.

This closes one source-to-volume provenance step, not B02 as a whole. Native
volume/instance/plate transform composition, complete job snapshots, worker/GUI
integration, hard containment and the hybrid pipeline remain pending. No physical
qualification or independent review is claimed. Linux run 36422010850 on older
042a11ac had completed dependencies and was building the app/tests at 13:44 UTC;
no Linux test pass is inferred from that progress.
