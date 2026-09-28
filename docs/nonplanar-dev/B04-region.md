# B04 owned native planar-region semantics

ADR-0027 and planar_region_contract_version=1 introduce a bounded adapter for
actual native LayerRegion perimeter/fill trees. The snapshot preserves native
scaled points, the captured scale, layer ID/height/print Z, roles, fixed bead
dimensions, mm3_per_mm, group hierarchy, reverse/sort flags, loop roles and inset
indices. Converted build-plate points carry an outward L1 coordinate-error bound;
native length and uncompensated path volume have separate arithmetic intervals.
The caller explicitly supplies the object-origin translation. No actual plate
membership, final movement order, deposited occupancy or export is inferred.

All source reads finish before callbacks. Nodes, points, recursion and time are
bounded. Any unsupported/invalid path or cancellation/staleness drops the whole
candidate. The admitted domain excludes bridge/gap-fill/support/ironing roles,
sloped/contoured/unknown entity types, arc metadata, nonzero native point Z,
disconnected/open groups, variable-width groups and skirt/unknown loop flags.
No material is silently omitted. Empty perimeter/fill roots are preserved.

Two initial positive tests fail before implementation (exit 42). Six final new
cases exercise real Orca body slicing; the independent 3-4-5/0.625 mm3 oracle;
mutated source/options; unsupported path/group records; late cancellation/stale
publication; deadlines/rounding; and destruction of the entire source Print in
the first callback. Combined with existing A05 transition tests, 251 assertions
pass with NoAssertions. JUnit records 25 case/section entries; the new test-case
count is six, not 25.

The initial test build used an integer argument that selected an unrelated
native contains() template; it was corrected to the intended double oracle.
The first positive native run also exposed an unjustified path-count >10 test
assumption: Orca combines infill into longer polylines. The test now checks the
actual preserved roles, hierarchy and points instead of an invented count.
Both failures remain in the evidence. Author review added captured scale and
loop/inset metadata, rejected unknown loop flags and removed a new compiler
warning by making the unsupported-type throw explicit. Independent review is
still pending; inherited native container/linker warnings remain.

macOS ARM64 Release application/native build and pinned-source audit exit 0.
Selected CTest executes 205/205 without skips. Six fresh OFF/ZAA comparisons pass
under the established timestamp/additive OFF-setting allowances. The final
reviewed source/binary hashes, commands, exit codes, XML, CTest discovery and
baselines are retained under evidence/B04-region. The older Linux run at 01865dcf
is still building in the retained observation; it is not evidence for this code.

Full body/cap partition, dense coverage, whole-object/plate/job binding,
sequential material and transitions, motion/flow filters and guarded export
remain unimplemented. Linux confirmation and physical qualification are open.
