# B08 material-motion author critical review

Scope: ADR-0063 and the bounded rigid material-motion API. This is an author
review, not an independent safety review or Gate B completion.

Reviewed source ownership separately from geometry acceptance. The preparation
factory recomputes public raw ledger geometry via the original validated capture;
its successful source has a private constructor. The motion factory owns all
components/policy/limits before callbacks. Neither a forged aggregate, callback
mutation nor a stale final callback can publish a protected successful result.
Results retain the checked original event and owned inputs even on collision.

Reviewed the continuous acceptance route independently of the negative probes.
Each component/deposition pair begins with its complete time/local-tool domain.
Closed midpoint splits cover their parents; only certified empty-annulus cells
or whole Upper-disjoint cells become leaves. Partial leaves are never published.
The tool bounding square alone cannot produce a collision; all negative probes
must be in the actual annulus or box. A wide-box test intersects material with
its edge while its centre is outside. A stationary-annulus test passes material
inside its empty opening, and refuses the narrower opening.

Reviewed time/material coupling. Current progress is an interval enclosing the
whole node time range; its Upper end inflation is not assumed monotone. Prior
rows use progress 1; future rows and pressure events supply no volume. Scalar
material callers use the original wrapper and representations. Angled varying-gap
rectangle/stadium cases, actual half-front native occupancy and native later-row
collision remain present. No CAD or Lower support volume replaces Upper geometry.

Reviewed uncertainty and termination. Outward clearance/input-uncertainty cubes
contain the required Euclidean neighbourhood. Combined policy/source numerical
error is checked outward against .05 mm. Every component, row, cell, predicate,
radial test and negative probe shares bounded work and the original deadline.
Cell/depth/work/time exhaustion, unsupported contact, stale revision, changed
rounding and final cancellation yield UNKNOWN without a protected PASS.
No original losses, negative tests, timeouts or packet precision were weakened.

Remaining risks: applicability to a complete measured head; static scene
composition; legal contact near deposition; short-packet flow/acceleration;
complete cap deficit/excess and order; source/job/plate/software/resource binding;
independent finite-byte replay and export. A direct Upper intersection is a
violation of the declared envelope, not a measured physical penetration depth.
An unresolved clearance band may remain UNKNOWN. A snapshot is an in-memory
proof only; complete persisted job/report/cache versions remain B13 work.
Current Linux and Windows execution and independent review remain separate.
Physical runs are NOT_RUN. Guarded export remains BLOCK.
