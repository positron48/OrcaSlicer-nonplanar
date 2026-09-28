# B03 connected nominal masks and holes

The invariant is preservation of edge-connected slope masks and every hole,
without merging pinched contacts or accepting intersecting projected boundaries.
ADR-0018 defines the bounded nominal domain. Upward source facets remain intact;
the analysis derives face ownership and directed loops in the owned mesh.
Every patch has exactly one outer loop and any number of holes. Interval signed
areas must agree with the selected triangle-area interval. Exact CGAL predicates
reject boundary crossings, overlaps and nonadjacent contacts across all patches.
Cancellation, revision and deadline checks cover boundary work before publication.

The initial three pipeline cases failed against the previous implementation
(exit 42). The implemented cases preserve a 3 x 3 plate's 1 x 1 hole (+9 -1 = 8
mm2), separate two 3 mm2 flat strips, and reject a valid solid whose slope mask
pinches at a vertex. Direct exact-predicate cases cover all endpoint permutations,
collinear overlap, a crossing at different Z, and a 1e-30 mm gap without snapping.

macOS ARM64 Release application/native targets build with exit 0. All 10
projection cases/151 assertions pass with NoAssertions; selected CTest executes
168/168 cases with no skips. Six fresh OFF/ZAA comparisons pass with only the
existing timestamp normalization and additive nptop_mode=off resolved setting.
Commands, exit codes, red/final XML, CTest discovery, baseline manifests and
source/binary hashes are in evidence/B03-patches. Author review checked the
boundary topology, orientation and whole-analysis cancellation paths; independent
review remains pending.

No curvature, finite footprint inset, head/scene access, import-error envelope,
body/cap partition, production planner/export or physical qualification is claimed.
These remain required work, together with real plate/job ownership and Linux CI.
