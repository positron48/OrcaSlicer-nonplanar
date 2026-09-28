# B02 source import audit (implementation pending)

Implementation follow-up: B02-mesh.md and B02-snapshot.md now record bounded
native topology/self-intersection checks, owned source bytes, SHA-256, pre-repair
geometry and exact repair comparison. B02-error.md establishes bounded decimal
conversion error. File capture, job budget integration and worker resource
isolation remain pending. The findings below describe
the pinned import path that motivated those changes.

Pinned native entry: Format/STL.cpp load_stl -> TriangleMesh::ReadSTLFile ->
admesh stl_open -> TriangleMesh::from_stl. Binary and ASCII parsing are native.
The load_stl path repairs before adding the mesh to Model.

TriangleMesh::from_stl currently disables the block copying original repair
statistics with `#if 0`; fill_initial_stats recomputes geometry metrics after
repair. Consequently TriangleMesh::repaired()/stats alone cannot establish that
an original input was unchanged or quantify the repair. This is a finding in the
pinned source, not a request to globally change stock import behaviour.

The guarded importer must preserve bounded source bytes, validate finite raw
facets before repair/shared-vertex hashing, and compare the original geometry to
the normalized native import. The initial policy can reject any repair; it must
not silently accept a changed mesh. Validate degeneracy, duplicate faces, edge
incidence/orientation, connected components, closed volume and self-intersections.
Existing MeshBoolean::cgal::does_self_intersect is a reusable native predicate,
not by itself a bounded/cancellable import pipeline. Budget exhaustion must block.

Native stl_open accepts a path and allocates from the parsed facet count, so a
file-size check alone is not yet a proven allocation/time bound. Capture a stable
private input for the parser; avoid reading the user's path twice as if two
potentially different contents were one snapshot. Units must be explicit for
fixtures; real STL units require user confirmation at import. Source file, original
Model and transforms remain untouched; derived analysis has separate ownership.

Planned observable tests: equivalent ASCII/binary block; open boundary; duplicate
and zero-area face; NaN/Inf; wrong orientation; intersecting shells; two components;
resource limit; source bytes/mesh/transform preservation. No B02 acceptance or
physical qualification is claimed by this audit.

Follow-up: the parser audit also identified unsafe optional metadata reads and
shared callback state. The bounded metadata prerequisite is tracked separately
in B02-parser.md; correcting it does not validate malformed mesh geometry.

Additional verified native details:

- admesh stl_read skips a facet when a vertex contains NaN, leaving the allocated
  zero/default entry. Inspecting only the repaired result loses that provenance;
  the guarded path must reject the original invalid input before accepting a mesh.
- MeshBoolean's vector/indices-to-CGAL conversion ignores add_face's return.
  Validate topology first and check an exact canonical triangle round-trip/count
  before treating does_self_intersect(false) as evidence about the original mesh.
  A silently dropped facet must never turn an invalid source into PASS.
- The native facet allocation is derived from file size (binary) or counted
  physical records (ASCII), not directly trusted from the binary header count.
  A byte bound is useful, but must be paired with explicit facet/work limits and
  worker isolation for non-cancellable CGAL calls; no resource guarantee is yet
  implemented by this source audit.
