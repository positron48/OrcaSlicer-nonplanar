# ADR-0024: bounded upper analysis through the native worker and diagnostic CLI

Status: implemented and native/CLI tests pass; independent review pending.

Extend the existing trusted native worker with an explicit upper-analysis request.
Protocol 2 binds both request kind and revision to the immutable source SHA-256;
old/inconsistent protocols fail closed. Geometry-only CLI behavior remains
available. `nonplanar_stl_audit --millimeters --upper INPUT.stl` additionally runs
the nominal upper analyzer inside the supervised child. The fixed slope limit
is 0.2, upward-facet cap 2500 and existing source/mesh/CPU/RSS/report limits remain.
No source file, preset, original model or printer is written.

Only aggregate nominal geometry is returned: upward/selected faces, connected
patches, holes, creases, affine patch count, area intervals, height range and
selected slope bound. Exact field sets/types/ranges and logical count/area
consistency are checked before accepting the bounded report. Cancellation,
deadline, request mutation, revision mismatch and unsupported parent arithmetic
cannot publish a partial upper result. Geometry validity is distinct from upper
analysis success; a hidden shelf can have valid topology and UNKNOWN projection.

The CLI report names the source-model frame and nominal-only scope, keeps
export_allowed=false, and uses exit 0 only for the requested completed analysis.
It is not plate placement, import-error-qualified surface bounds, head access,
path planning, a full JobSnapshot or guarded export. The worker and parent must
be rebuilt together for protocol 2; no persistent profile/project is migrated.

Predefined positive: the pinned 30 x 20 x 4 block has 32 upward/selected faces,
one affine patch, no holes/creases, area 600 mm2 and Z=4. A separate 28-face
C-shaped extrusion has valid closed geometry but overlapping upward projections
and must remain UNKNOWN. Native tests cover owned bytes/options and malformed
summary counts/ranges/schema. CLI tests retain all existing geometry cases and
add the actual upper-analysis invocations plus missing-unit/usage refusals.
