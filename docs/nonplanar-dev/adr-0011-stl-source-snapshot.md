# ADR-0011: preserve bytes before invoking native STL repair

Status: internal bounded API; production import integration pending.

The pinned STL importer repairs before Model insertion and discards part of the
repair statistics. Treating the resulting TriangleMesh as an unchanged source
would lose both geometric and diagnostic provenance. Importing a user pathname
twice would also permit the first and second reads to refer to different bytes.

The guarded path therefore takes a bounded byte view, copies it before any
callback, and creates a const source snapshot with SHA-256 and explicit unit
declaration. Native facet parsing consumes an anonymous stream containing that
exact snapshot. Structural preflight limits bytes/facets before native allocation;
the native reader continues to perform coordinate parsing. This is a separate
bounded entry, not a replacement for ordinary Orca import or a second slicer.

The pre-repair mesh is retained separately. Geometry validation runs first on
those parsed coordinates. Native repair runs only on a copy, retains its own
statistics and must preserve the exact multiset of oriented triangles. Changes
to vertex indexing, face order and normals do not move the surface. Snapping,
flipping or removing geometry invalidates the derived result. Callbacks,
cancellation, deadline expiry or exceptions never publish a partially accepted
derived mesh; the captured input remains available for diagnostics.
Each operation also owns its limit values before invoking callbacks; mutation
of the caller's configuration cannot relax a running audit or import.

This contract deliberately stops before GUI/CLI file capture, persistence,
transforms and whole-job qualification. ASCII conversion error and process-level
CPU/memory limits still need their own evidence. No cache/persistent schema is
introduced in this step, no user Model/preset is rewritten, and the current
hybrid output gate remains closed. Tests exercise the native API directly.
