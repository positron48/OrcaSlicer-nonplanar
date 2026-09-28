# ADR-0012: bound actual native mesh-placement coordinates

Status: implemented and tested on macOS ARM64; internal bounded API, no job/export approval.

The pinned TriangleMesh::transform(Transform3d) calls native its_transform, which
calculates transformed coordinates and casts each vertex back to binary32. A
finite double matrix alone is not a bound on the resulting placed surface.

The placement boundary takes an already captured/audited STL result and an
explicit model-local-to-build-plate affine matrix. The supplied binary64 matrix
entries are authoritative input values, copied before callbacks. This does not
derive a matrix from mutable Model/plate state, certify earlier matrix composition
or change upstream Model centering/placement behavior. A future job adapter must
capture that provenance and budget any preceding transformations separately.

Run the existing native TriangleMesh::transform on an owned copy. Independently
evaluate each affine coordinate using outward binary64 intervals, starting from
the exact parsed binary32 vertex. Compare the actual native binary32 result with
that interval. The maximum vertex L1 error bounds Euclidean displacement of all
corresponding triangle points. Amplify the original source-error bound by an upper
bound on the matrix's induced Euclidean norm, sqrt(norm_1 * norm_infinity), and
charge the sum to the caller's placement allowance. It is not a solid-occupancy
certificate or an allowance that may be charged independently at every stage.

Only finite affine matrices with a proven positive determinant are supported;
singular, reflected and numerically unresolved transforms are UNKNOWN. Matrix
coefficients, source/placed coordinates, vertices, faces and work time remain
bounded. Re-audit the actual transformed mesh, because float rounding can collapse
or change topology. Missing provenance/error bounds, unsupported arithmetic,
budget exhaustion, stale revision, cancellation or failed geometry discard the
accepted output. Original bytes, mesh and supplied matrix are never mutated.

The operation runs inside the future analysis worker/job context, not a GUI
callback. Its native geometry calls have cooperative checks, not hard per-call
deadlines. No new persisted project schema, Verified state, export path or OFF/ZAA
behavior is introduced. Independent review remains required.
