# B02: native mesh placement with an explicit error bound

Invariant: placing an audited mesh must not silently discard source uncertainty
or native coordinate rounding. ADR-0012 specifies the exact mathematical scope
and the remaining Model/job integration boundary. The implementation reuses
TriangleMesh::transform and rechecks its actual binary32 output.

Input is an accepted immutable STL import result, an authoritative binary64 affine
matrix from model-local to build-plate coordinates, revision and explicit limits.
Both source handles and all matrix/limit values are copied before callbacks. The
result retains an immutable source/matrix/revision snapshot and, only on acceptance,
a separate audited placed mesh. No source, original geometry, caller matrix or
preset is changed. A callback cannot relax a running allowance or substitute a
different source/matrix. Current revision and cancellation are checked through the
calculation and before acceptance; changed floating-point mode also rejects.

Affine coefficients are finite and at most 100 in magnitude, translations at most
10000 mm. The homogeneous last row is exactly [0,0,0,1]; an outward determinant
interval must prove positive orientation. Singular, reflected, projective and
unresolved cases stay UNKNOWN. The existing geometry limits bound input and output.
Actual float conversion can collapse valid source triangles; the native placed
mesh must pass a new complete B02 geometry audit after the rounding calculation.

For each source vertex, an outward interval encloses each exact affine coordinate.
Its difference from the actual stored native coordinate yields a conservative L1
rounding bound. Taking the maximum over vertices bounds all corresponding triangle
interiors by barycentric interpolation. Original Euclidean source error is amplified
by sqrt(norm_1(A) * norm_infinity(A)), an upper bound on the induced Euclidean norm.
Both components and their outward sum are reported separately. The sum must fit
the supplied allowance (default 0.01 mm; at most 0.05 mm in this internal API).
This does not allocate a fresh independent whole-job budget to every operation:
the eventual job must account for these consumed bounds plus every other stage.

The source conversion proof and this proof concern corresponding surfaces, not
solid-occupancy containment. A supplied matrix is an input, not evidence that native
Model centering, sequential volume/instance transformations, plate choice or earlier
matrix composition were captured correctly. Those adapter steps remain pending.
The native geometry calls are cooperative and belong inside a supervised analysis
context before GUI integration. No new worker protocol, persistent project schema,
Verified state or hybrid-export permission is introduced.

Tests cover an exact 90-degree rotated/scaled block with independently calculated
bounds and volume, nonuniform error amplification, a rational binary64-to-binary32
translation oracle, explicit allowance rejection, invalid matrices/provenance,
native topology collapse, caller mutation, cancellation, staleness and arithmetic
environment changes. The Fraction oracle uses fixed rational native output values
and proves the C++ regression constant without importing implementation code.

## Observed validation

macOS ARM64 Release builds with exit 0. Six focused cases pass 110 assertions
with NoAssertions; selected CTest executes 125/125 without failures/skips. The
independent Fraction calculation proves the exact corner error
30923764531/36028797018963968 mm and its exact representability as the C++ test
constant. Commands/exits, native XML, oracle, compiler/platform and source/binary
hashes are in evidence/B02-placement. The normative bundle audit passes.

No new full app/OFF/ZAA capture: the placement API is exercised by native tests
and has no production caller; ordinary stock placement/slicing/writer code is
unchanged. The six B02-snapshot baseline comparisons remain the latest evidence.
Linux/Windows execution, GUI/job integration, independent review and physical
qualification remain pending; this result does not close B02 or Gate A.
