# B03 exact nominal affine segment heights

ADR-0028 extends the whole-footprint affine query with outward endpoint Z and
gradient intervals, reference face and revision. The reference face must contain
the start in its exact XY projection. Its plane is evaluated with EPECK rational
constructions only after the entire swept disk passes mask and crease clearance.
Coplanar triangulation boundaries do not prevent a positive result; a crease or
hole remains blocking. Publication happens after the final cancellation/revision
check. Exceptions reset both the status and optional bounds.

Two new predefined cases initially fail with missing height data (exit 42).
The implementation passes independent z=2+x/8 and z=2+x/3 oracles in both
directions and at stationary points, including gradient bounds and cross-facet
segments. A ramp inside a non-affine patch proves that the actual containing
face is selected rather than the first face of the patch. Boundary and crease
crossings and existing late-callback regressions retain no height bounds.
Generic XY-only containment still supplies no Z/curvature result.

macOS ARM64 Release application/native build exits 0. All 24 B03 cases/630
assertions pass with NoAssertions. Selected CTest executes 207/207 without skips;
six fresh OFF/ZAA comparisons pass under the established timestamp and additive
OFF-setting allowances. Commands, exit codes, red/final XML, discovery, baselines
and source/binary hashes are in evidence/B03-heights. Author review checked the
local-sheet argument, exact plane arithmetic and final publication. Independent
review, import-error-qualified heights, curved surfaces and full planning remain
pending; nominal heights do not authorize printing.

Linux run 36434944242 on older 01865dcf completed native application/test build
successfully, then failed the inherited Skirt height is honored case with
Coordinate outside allowed range. Public bounded annotations and job timings
are preserved. The diagnostic CLI step was skipped because CTest failed.
This is neither a successful Linux gate nor a test of the current revision;
the resulting initialization investigation and local correction are recorded
in `A02-print-defaults.md`, with Linux confirmation still pending.
