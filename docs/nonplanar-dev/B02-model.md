# B02 native Model transform capture

Invariant: source centering, native volume placement, native instance placement
and an explicit plate-frame translation form an owned sequential geometry
calculation with an accumulated error bound. ADR-0015 defines the exact scope.

Four new cases compare the result with actual Model::mesh geometry after native
rotation, scale and translation, followed by the explicit plate transform. A
separate numeric case requires a 0.0001 mm input allowance to grow by at least
six under consecutive uniform scales two and three, and rejects an insufficient
budget. Negative selections/transforms cover extra/missing objects, volumes and
instances, nonprintable instance, zero scale, reflection and a NaN plate origin.
Callbacks delete the entire original Model and mutate the source, frame and
limits without changing the captured result; a late stale revision rejects all
final geometry/error output. Initial positives failed (exit 42, three failures).

The original single-affine implementation was extracted into a private kernel,
retaining its source checks through the original public wrapper. Fourteen focused
placement/centering/model cases pass with 227 assertions and NoAssertions enabled.
The final macOS ARM64 Release app and test targets build with exit 0; fresh selected
CTest executes 153/153 without disabled/skipped cases. Six fresh OFF/ZAA comparisons
pass with the existing timestamp rule and sole nptop_mode=off schema addition.
Exact commands, failures, final XML and source/binary hashes are in evidence/B02-model.

This provides the native Model transform portion of B02. Plate selection and
membership still require a real GUI/job adapter; the supplied frame here is not
operator confirmation. Worker transport, complete immutable job state, hard
resource containment, surface/cap planning and the hybrid pipeline remain open.
No Linux test pass, physical qualification or independent review is claimed.
