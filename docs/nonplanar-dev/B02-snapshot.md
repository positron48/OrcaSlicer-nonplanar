# B02: owned STL bytes and native import provenance

Invariant: the bytes hashed and parsed are one owned snapshot; native repair
cannot turn a changed or previously invalid source into accepted geometry. This
is an internal native API exercised by tests. It is not the completed GUI/CLI
import transaction or a hybrid export route.

`import_stl_snapshot` first copies at most 2 MiB into a const snapshot and records
its SHA-256 using the already linked OpenSSL implementation. It retains both
that source and a const pre-repair native mesh. Units are an explicit millimeter
declaration supplied by the caller; tests declare their analytical fixtures, not
an operator's unmeasured file. A source exceeding the byte budget is not copied.

The bounded admesh entry reuses the native facet reader from an owned anonymous
temporary stream, without opening or reopening a caller pathname. Binary record
count and exact byte length must agree, even when the 80-byte header starts with
`solid`. ASCII input is one bounded solid with seven records per facet, an end
marker and no trailing non-whitespace content. Geometry lines are at most 96
bytes and header lines at most 255 bytes; LF and CRLF are supported. The bounded
record preflight counts at most the configured 5000-face ceiling before native
allocation. Coordinate parsing remains native; a second STL geometry engine is
not introduced. Some permissive stock STL variants are intentionally rejected.

Finite raw coordinates and their domain are checked before shared-vertex
construction or repair. The geometry audit from B02-mesh.md must pass on the
unrepaired native mesh. Only then is a separate stl copy sent through native
repair; original repair counters are retained directly, bypassing the pinned
TriangleMesh stats loss. Exact oriented-triangle comparison must preserve every
coordinate, winding and facet multiplicity. Normal recomputation and exact
vertex sharing are permitted; snapping, face removal and flipping are not
silently approved. There is no tolerance-based repair or user-model mutation.

The ordinary STL entry retains its existing successful-input semantics. Shared
reader error handling now rejects incomplete coordinate scans or failed line
reads rather than asserting on untrusted input or inspecting an uninitialized
line buffer. Strict finite-coordinate rejection is enabled only for the new
snapshot entry; ordinary stock repair policy is unchanged.

Limits still have explicit boundaries: capture from a user file descriptor,
source-path identity, ASCII decimal-to-float error budget, transformations, hard
worker CPU/memory/time limits, persistence and job revision/invalidation are not
implemented here. Geometry acceptance describes the parsed floats and does not
certify fidelity to exact decimal surfaces or qualify a full imported job. Native
repair and CGAL calls are cooperatively checked before/after; they still require
process isolation before untrusted interactive integration. B01 blocks output.

Native tests cover equivalent ASCII/binary analytical cubes, CRLF, known SHA-256
`abc`, callback mutation of the caller's buffer, byte/facet/domain bounds, units,
cancellation/exceptions, count mismatch, truncation, NaN/Inf, malformed records,
unconsumed bytes and invalid topology before repair. Oriented-multiset tests
distinguish face-order/cyclic-index changes from reversed winding, coordinate
changes and lost facets. Independent critical review and physical evidence remain
pending; observed build/test/baseline results are recorded after execution.

## Callback limit regression

Author review identified that a callback could modify the caller's referenced
MeshAuditLimits after initial validation. A native negative case demonstrated
both audit_mesh and import_stl_snapshot accepting a 20 mm cube despite their
initial 1 mm coordinate limit (six failed assertions, exit 42). Both entries now
capture their own limits before callbacks. The same test rejects with
COORDINATE_DOMAIN and no derived accepted mesh. This fixes the e1a718ef audit
API as well as the new importer; neither API had a production output caller.
The red XML and exact commands remain in evidence/B02-snapshot. This is an
author finding, not an independent review approval.

## Observed validation

macOS ARM64 Release application and all selected test binaries built successfully.
Seven new snapshot cases / 119 assertions pass with NoAssertions. Selected CTest
executes 107/107 with no failures/skips. The existing upstream STL scenario passes
8 assertions; unrelated upstream scenarios were not run. Six fresh isolated,
offline CLI OFF/ZAA outputs match stock G-code and XYZ/E/F/order replay. The only
resolved-config difference is the documented additive B01 OFF default. Source
and binary hashes, native XML, exact commands/exit codes, the red limit regression
and baseline manifest are in evidence/B02-snapshot. The normative bundle audit
passes. No goldens or margins were changed. Linux CI is still building an earlier
commit; no Linux, ASan, independent review or physical pass is claimed here.
