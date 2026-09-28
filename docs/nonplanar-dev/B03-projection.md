# B03 bounded nominal upper projection

Invariant: a hidden upward-facing facet must not be silently accepted as an
externally visible top, and cancellation/staleness must not publish a partial
mask. ADR-0017 records the exact domain, proof and predefined expectations.

The native analysis audits and owns the mesh, identifies all upward faces using
CGAL exact predicates and checks pairwise projection interiors. Touching edges
are valid; any overlap gives UNKNOWN for the entire analysis. Exact predicates
live in the existing CGAL target behind a small native adapter; audit/interval
orchestration lives in libslic3r, preserving the dependency direction. No new
geometry dependency or scaled-coordinate projection is introduced.

Each retained face has its mesh index, interval-bounded nominal XY area and
slope upper bound. The preliminary slope filter retains excluded source faces
and reports excluded coverage through full versus filtered area. These are
nominal mesh bounds only, not a curvature, import-error, finite-footprint,
head/scene-clearance, support, transition or printability certificate. No GUI
or guarded-export route is enabled.

Initial four cases failed against an UNKNOWN-only implementation (four failed
assertions, exit 42). The first implemented run passes 4 cases/71 assertions.
The native STL/Model integration uses the immutable 30 x 20 x 4 fixture with
32 top facets, volume X scale 2, instance Z translation 6 and an explicit plate
frame. Expected area 1200 mm2 passes. An initial expected Z=8 was incorrect:
ModelVolume::center_geometry_after_creation translates its mesh by -2 and its
volume transform by +2, preserving original top Z=4; adding instance Z=6 gives
Z=10. Both compensating offsets are now asserted before checking Z=10. Only the
test expectation changed, with failed XML/CTest evidence retained.

Final macOS ARM64 Release application and native targets build with exit 0.
Six projection cases/103 assertions pass with NoAssertions enabled, including
late stale revision, zero revision rejection and an elapsed deadline. Exact expected face sets verify
the analytic masks without duplicate facets. Selected CTest executes 164/164
without skips/disabled cases, and all six fresh OFF/ZAA comparisons pass with
the existing timestamp normalization and sole additive nptop_mode=off schema.
Commands, exit codes, red/final results and source/binary hashes are retained in
evidence/B03-projection. No Linux result or independent review is inferred.
Software remaining includes patch grouping, curvature, ROI inset, actual
head/scene access, body/cap reservation and full job ownership.
