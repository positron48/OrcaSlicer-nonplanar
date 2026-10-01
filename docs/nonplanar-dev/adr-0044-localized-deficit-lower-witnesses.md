# ADR-0044: localized missing-volume lower witnesses

Status: bounded localization implemented; deficit resolution, complete B07
and independent safety review pending.

ADR-0043 separates whole-domain nominal missing and outside volume. A planner
also needs regions in which additional material is demonstrably necessary.
Add protected internal material-deficit contract 1, without changing existing
material/IR/project/profile serialization, fixtures or export routes.

Input is an owned MaterialFillSnapshot and two finite strictly ascending cut
lists spanning its exact target XY rectangle. Their Cartesian product is a
complete closed grid; cell interiors are disjoint and boundaries have zero
volume. The snapshot factory retains the protected fill proof, complete grid,
regional target intervals, upper bounds on possibly available nominal amount
and covered volume, and a missing-volume lower witness for every region.
Caller-edited wrapper reasons/provisionals cannot replace the proof. A zero
lower witness never proves fill or absence of a defect.

For region j, reuse the protected target integral's exact convex clipping and
polygon moments to enclose V_j above the actual nominal body roof. Sum the
endpoints exactly and require their complete-grid interval width to meet the
requested mm3 precision. No supporting plane substitutes for the actual roof.

For every laid candidate bead whose conservative nominal XY footprint has a
positive-area intersection with j, count its entire commanded amount times
actual current fraction. Exact finite longitudinal clipping retains rotated,
reversed and fractional butt planes. Pure boundary intersections have zero
volume. Future records are absent; travel/retraction contribute no material.
Reuse the existing nominal roof projection as an outer XY enclosure, rather
than inventing a sampled or axis-aligned footprint test.

This exact sum S_j overestimates candidate material in a partial intersection
and every overlap; it also counts any material outside the target vertically.
It is deliberately an upper bound, never local occupied/covered volume.
Define covered_upper_j=min(S_j.upper,V_j.upper,whole_covered.upper), and
missing_lower_j=max(0,V_j.lower-covered_upper_j), using outward arithmetic.
Since true covered_j is no larger than any of these three upper bounds,
missing_lower_j is a rigorous lower bound on missing target volume in j.
The exact sum of witnesses bounds missing volume over their disjoint regions
and must not contradict the source whole-missing upper bound.

The witnesses do not exhaust the whole deficit, estimate its shape within a
cell, localize vertical excess, prescribe an extrusion quota, or certify a
repair path. The requested precision constrains the regional target quotas,
not an artificially narrow interval around these one-sided witnesses. Fine
grids may weaken a witness because intersecting whole amounts are counted
conservatively; that does not allow a fill PASS. Later planning must check
actual unoccupied geometry and finite-width/contact constraints before adding
material, and re-evaluate all changed sources.

Default policies are 256 regions, 4095 proof fragments, 200000 evaluations,
0.001 mm3 target-sum interval width and a one-second cooperative deadline.
Hard ceilings are 256 regions, 65535 proof fragments, 200000 evaluations and
30 seconds. Input list sizes are checked before bounded copies. Proof-cell
clipping and candidate footprint queries share one work/fragment/deadline
budget. Missing, incomplete, unordered, nonfinite or oversized input, precision
failure, exhausted work/cells, stale revision, cancellation, timeout or changed
rounding publishes no snapshot. Both source revisions are checked before
publication. No cache is persisted; future cache keys must bind this contract,
software, fill proof, both source prefixes, cuts, numerical/resource policies
and full job dependencies.

Three analytical cases / 118 assertions prove current/future exclusion,
independent rotated finite-butt localization, correct-fill zero witnesses and
owned callback inputs. The actual native case locates positive deficit in all
four quarters of the first-pass 1:16 wedge domain with a combined lower witness
of approximately 0.98329 mm3. The whole deficit remains approximately
1.27173-1.28710 mm3. Localizing a strict lower part is useful planning evidence,
not complete reconstruction or accepted deposition. Independent whole-native
geometry review, finite-width/contact, perimeter/seam, later actual support,
curved paths, motion order/limits and final-byte replay/export remain pending.
The public guarded-hybrid blocker remains closed.
