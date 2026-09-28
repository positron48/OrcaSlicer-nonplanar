# ADR-0015: captured native volume and instance transform chain

Status: implemented and locally tested; internal bounded geometry API.

Native ModelObject::raw_mesh writes volume-transformed coordinates to binary32,
and ModelInstance::transform_mesh performs another binary32 write. Multiplying
the two matrices first does not reproduce those intermediate rounded meshes.
The centering bound from ADR-0014 must also be propagated through both stages.

capture_model_placement accepts one printable object, one model-part volume and
one printable instance. While the caller owns/synchronizes that Model, capture
its resolved volume/instance binary64 matrices and the caller-supplied plate
frame; then capture the centered volume before any callbacks. No original Model
reads occur after callbacks begin. The snapshot owns those matrices/frame and
the centered source data, including source bytes, source offset and revision.

Apply the volume and instance matrices in that order through the actual native
TriangleMesh::transform boundary, then an explicit world-to-plate translation.
Each stage reuses the bounded affine error/audit kernel from ADR-0012. Propagate
the incoming Euclidean error by an upper bound on that matrix's induced norm,
then add the newly measured native rounding allowance. Keep the centering and
three transform-rounding components separately; the reported total additionally
includes the source error and any amplification of earlier components.

Matrices are authoritative resolved native values. This does not certify ideal
trigonometric matrix generation from user angles. The plane origin is an explicit
caller input, not a verified GUI PartPlate selection or membership test. Only
positive-determinant bounded affine transforms are supported; reflection,
singularity, nonfinite inputs and unqualified source/selection fail closed. The
coordinate/resource limits apply at each intermediate stage as well as the end.

All stages consume the same overall cooperative deadline. Cancellation/staleness
is checked between stages and before accepting the final geometry. Partial stage
meshes/error fields do not become accepted output when a later stage fails. The
existing direct imported-mesh API still requires unchanged native-import
provenance and uses the same private kernel; no transformed mesh is mislabeled
as an untouched STL import to reuse that API.

The function follows native model geometry operations; it does not qualify the
planar slicer's separate scaled-coordinate conversion, full scene/firmware
coordinates, occupancy, whole-job ownership/hash protocol or export. The future
GUI/worker adapter must provide the actual plate frame and revision binding.
No production job caller or physical/independent-review approval is added.
