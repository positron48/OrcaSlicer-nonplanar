# ADR-0027: consume owned native source geometry in placement

Status: implemented and native tests pass; independent review pending.

The native placement primitive previously accepted a live Model. The pre-apply
source in ADR-0026 is immutable and should drive downstream geometry even if
GUI state changes or disappears. Add capture_input_placement using its captured
NativeInputBinding, original StlImportResult and explicit PlateFrame.

Require one printable object/model-part volume/inside instance, nonzero revision,
valid fixed geometry limits and the source model's captured plate index matching
the supplied plate index. Reject resource limits before reconstructing a view.
Reconstruct a private native Model solely for placement, copying the captured
mesh without centering again, exact volume/instance/source matrices and source
unit/offset metadata. This reuses capture_model_placement, native TriangleMesh
transforms and existing independent error-accounting tests. It introduces no
alternative geometry engine or second config resolution.

ModelPlacementSnapshot retains the NativeInputSnapshot pointer for this path,
alongside owned original STL bytes, centered mesh, matrices, plate and revision.
Legacy live-Model captures retain an empty native_input field. This internal
aggregate has no persisted serialization/cache consumer; job identity consumers
must explicitly bind the raw source fingerprint before accepting analysis.

Capture copies bindings, source and limits before any callback. Reconstruction
latency is deducted from the total deadline, with a final elapsed-time check.
Failures propagate their geometry status/reason but discard snapshots, accepted
geometry and error bounds. Unsupported source conversions/meshes or mismatched
STL still reject through the existing native centering/source audit. Exact
geometry/bytes/config ownership remains separate from compatibility approval.

Two native cases prove original native placement after source Model deletion
and callback mutation of the caller's binding, source preservation and valid
nominal upper projection. Negative cases cover missing/revision-zero bindings,
wrong selected plate, unrelated STL, unprintable instance, cancellation, late
staleness, elapsed deadline and callback failure. The supplied plate origin is
still a caller-owned declared frame: actual GUI plate membership, its setters,
full-job profile/scene/software binding and hard worker containment are pending.
This geometry result never approves processing/export or physical printing.
