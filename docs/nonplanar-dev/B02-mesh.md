# B02: bounded native mesh geometry audit

Invariant: accepting a parsed mesh must not silently repair its geometry, change
the original, or validate a subset after conversion drops a face. This module
audits an owned copy of native `indexed_triangle_set`; it grants no export or
printer qualification. Source bytes, decimal import error, transforms, placement,
support and a complete B02 import transaction remain separate requirements.

The caller must declare millimeters. Finite coordinates and valid indices are
checked before exact native vertex welding. Only identical coordinates merge;
there is no tolerance repair. Repeated/degenerate triangles, open/nonmanifold
edges, inconsistent or inward winding, disconnected shells and disconnected
vertex fans block acceptance. Interval arithmetic bounds nonzero area and signed
volume under the existing strict floating-point environment. An unresolved
area/volume is UNKNOWN. Positive volume alone is not a self-intersection test.

After topology checks, the existing native CGAL conversion must round-trip the
complete multiset of oriented triangles exactly (cyclic vertex rotation is
allowed; reversed winding is not). Then the native CGAL self-intersection
predicate checks that same geometry. The result owns its const derived mesh;
source arrays and cached statistics are untouched. The bounded domain is at most
5000 faces, 15000 vertices and absolute coordinates of 10000 mm; callers may
lower these limits. No statement is made about arbitrary meshes or SYS-08.

Cancellation/deadline checks are cooperative, including checks immediately
before and after CGAL. A late result is UNKNOWN. CGAL itself cannot be interrupted
by this API; a separate worker with hard resource limits is required before
connecting this module to an untrusted interactive import. The current callers
are native tests. The B01 production gate continues to reject hybrid output.

Positive tests use an independently known 20 x 10 x 2 cube (400 mm3) and exact
duplicate vertices whose normalization preserves the source. Negative tests
cover invalid indices, NaN/Inf, missing/duplicate/degenerate faces, orientation,
units, work limits, cancellation and separate/overlapping shells. A connected,
closed extruded bowtie has positive signed volume but intersecting opposite
walls at (1.2,1.2,z); it must reach SELF_INTERSECTION, rather than be rejected
only as two components.

Validation results are recorded below after execution. Independent critical
review remains pending. There is no new stock slicing/writer/import caller, so
the six fresh OFF/ZAA captures recorded in B02-parser.md remain the latest stock
path evidence; this module does not claim a fresh application baseline run.

## Observed validation

macOS ARM64 Release: six MeshAudit cases / 54 assertions pass with NoAssertions.
Selected native CTest executes 100/100 without skips/failures. The native
nonplanar/FFF test targets build successfully; the application was not rebuilt
for this otherwise unreferenced module. The first build failed on a nested
array initializer; its exit 1 remains recorded. Final source/binary fingerprints,
exact command/exit records and native XML are in evidence/B02-mesh. The normative
package audit passes. Linux application CI for an earlier commit remains running;
no Linux result for this change is claimed.
