# ADR-0014: source-to-native-volume centering provenance

Status: implemented and locally tested; bounded internal background API.

Pinned ModelVolume::center_geometry_after_creation translates binary32 mesh
vertices by the negative float-cast bounding-box center, while storing the
binary64 center in source.mesh_offset and compensating it in the volume transform.
That is a distinct rounding step before the affine map covered by ADR-0012.
ModelObject::raw_mesh and ModelInstance::transform_mesh may add later transforms;
this boundary does not certify those or their composition.

capture_centered_volume takes a checked STL import and a synchronized native
ModelVolume. It requires a model part, ordinary millimeter source metadata, no
unit conversion/builtin/reload transform, and an exact matching source offset.
It owns a copy of the current volume mesh before invoking callbacks. Source bytes,
offset, source-error allowance and revision are retained in immutable storage.
No ModelVolume reads occur after the first cancellation/revision callback.

Re-audit the captured current mesh. Reproduce the pinned native centering operation
on the checked import mesh, then require exact oriented-triangle equality with
the current volume, ignoring only face order and cyclic corner rotation. This
proves geometric correspondence; matching a filename or metadata alone cannot
substitute for it. No source/project meshes or offsets are rewritten.

For each imported binary32 vertex, evaluate (p - stored_binary64_offset) minus
the actual centered binary32 vertex using outward intervals. The maximum vertex
L1 bound covers Euclidean displacement at every corresponding triangle point.
Add the STL conversion bound once: centering is a translation with unit gain.
The stored offset is authoritative; this does not claim an independently exact
center of the original decimal source. Later volume/instance/plate transforms
must propagate this bound and account for their own arithmetic before use.

Limits are inherited from the bounded mesh contract: at most 5000 faces/15000
vertices, 10000 mm audited coordinates and cooperative work limits. Missing
provenance, malformed geometry, mismatch, unsupported metadata, stale revision,
cancellation, unsupported arithmetic, deadline or exceeded error allowance cannot
return accepted geometry/bounds. CGAL calls and allocation are not hard-contained
by this internal function; the future worker/job adapter remains responsible.

No whole Model/plate snapshot, transform-chain proof, solid occupancy certificate,
GUI badge or export approval is introduced. Native import integration is tested,
but the helper has no production GUI/job caller yet. Independent review remains
required before qualifying this numerical boundary.
