# ADR-0017: bounded nominal upper projection

Status: implemented bounded geometry analysis; independent review pending.

B03 starts with a geometric sub-contract, not a tool-accessibility decision.
The input is a mesh declared in millimeters in the caller's supplied frame.
Native B02 audit must first establish a closed, consistently oriented,
non-self-intersecting single component. The analysis owns that normalized mesh
and identifies facets by its indices. This is not CAD/source-byte provenance,
plate selection or a whole-job fingerprint.

Project every upward-facing facet onto XY. Use exact CGAL orientation predicates
on the nominal binary32 vertex coordinates. Two counterclockwise triangles have
disjoint interiors if an edge of either triangle separates all vertices of the
other to its right or onto the edge. Shared boundaries remain legal. If any two
upward interiors overlap, return UNKNOWN for the entire analysis; splitting a
partially hidden facet is not yet implemented. For a closed outward solid, an
occluded upward interior has an upward boundary above it along the vertical
ray. Disjoint upward interiors therefore define the nominal upper heightfield.
The contract concerns facet interiors; finite footprint and boundary margins
remain separate and mandatory before a print plan.

For each upward face retain the owned mesh face ID, outward interval bounds for
projected area and an upper bound on nominal slope sqrt(nx^2+ny^2)/nz. The caller's
max_slope is only a preliminary filter. Retain excluded facets and report both
full and filtered area. A flat face reports zero slope exactly. All arithmetic
requires the existing strict interval environment. No measured angle, curvature,
tool/scene clearance, import-error envelope, support or transition qualification
is inferred from those nominal values.

Limits and mesh are captured before callbacks can alter them. Cancellation,
deadline, stale revision, invalid arithmetic environment or budget overflow must
publish no partial mask. Audit and projection share one cooperative overall
deadline. This remains an internal worker-ready API; actual job/GUI ownership,
hard resource containment, patch grouping/curvature, inset masks and tool checks
remain open. Neither a heightfield nor a nonempty slope mask permits export.

Predefined tests (recorded before implementation): a 20 x 10 x 2 mm block and a
20 x 10 mm binary-exact wedge z=2+x/8 have full projected area 200 mm2 and two
upper triangles. Their fixed interior ROI [2,18] x [2,8] is 96 mm2 and lies fully
inside this mask; no path coverage is claimed. Wedge slope is 1/8 and height span
2.5 mm. A slope limit 0.1 excludes all 200 mm2 but preserves the facets and source.
A connected C-section extruded through 10 mm has a roof over its lower shelf and
must return UNKNOWN, never classify the hidden shelf as visible. Mesh, budget,
units, callback mutation, stale revision and altered rounding are negative cases.
