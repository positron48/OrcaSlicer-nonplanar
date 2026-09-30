# ADR-0028: one exact corefinement for reserved body and cap

Status: contract fixed for implementation; independent review pending.

Use the existing native CGAL dependency and indexed TriangleMesh representation
for body/cap partition before native Print. Existing MeshBoolean::cgal wrappers
use the inexact-constructions kernel, so they cannot supply the required point
conversion bounds. The new isolated Nonplanar backend uses the dependency's
exact-predicates/exact-constructions kernel, with one native corefinement producing
M minus reservation and M intersect reservation. No planar slicing engine or
upstream mesh boolean behavior is replaced.

Inputs are owned placed STL/source/revision and a closed selected reservation
solid in build-plate coordinates. The reservation is derived geometry, never a
mutation of the source. It can reserve a partial mask, curved roof or stepped
interface; selection/tool access still belongs to B03/planner. A partition result
alone does not qualify the reservation as printable upper material.

Require exact positive closed body/cap volumes and exact original=body+cap
volume identity from the common corefinement. Convert each exact output coordinate
to native binary32 and bound its L1 displacement using the exact CGAL coordinate's
enclosing binary64 interval. Audit native outputs for topology/orientation/self
intersection after conversion; collapsed/invalid outputs reject. Carry inherited
source/placement error plus the maximum partition conversion error as a distinct
outward total bound. Record exact and native volume intervals and their conversion
error separately; volume agreement alone never establishes spatial partition.

The common exact boundary is the nominal partition oracle; no independently
rounded sequential difference is used to reconstruct the cap. A shared exact
point map supplies identical binary32 coordinates to both meshes. Internal faces
must have identical vertex triples on both sides; unmatched faces must lie on
the original surface. A different internal triangulation rejects before rounding
can create a gap or overlap. Immutable output
binds source byte/native input identities, placed source, reservation and both
derived meshes to the current revision. Changing the reservation invalidates its
identity and all dependent native body/material/path products. Fixed input/output
mesh bounds and cooperative cancellation/deadline/current-revision checks apply.
Opaque CGAL corefinement still needs worker containment before interactive import;
this module does not claim a hard kernel deadline or process-memory bound.

Positive tests must include interior reservation preserving surrounding walls,
through-hole geometry, nonconstant roof/stepped geometry, original preservation,
independent analytic volumes and occupancy samples. Negative tests cover invalid
reservation/source, empty/full reservation, conversion budget, stale/cancelled/
late work and numerical/aggregate limits. Then derive native body trajectories
from this body mesh, not from the complete original mesh. Sparse under-cap support
and boundary seam/material proof remain mandatory subsequent B04/B05/B06 checks.

No public project serialization or MotionEvent v1 changes. Internal partition
contract/fingerprint schema 1 must be bound by future job/cache consumers; no
partition or native path snapshot is export approval.
