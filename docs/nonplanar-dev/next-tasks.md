# Next implementation tasks

Current source checkpoint is `status.json`; milestone reports below distinguish
bounded implemented primitives from the complete pipeline. Sphere evidence and
digital coupon preparation are recorded in `sphere.md` and `coupons/README.md`.
They do not qualify the physical printer. Gate A remains open.

Next software tasks, in dependency order:

1. Complete B01 compatibility and whole-job provenance, then B02 original STL
   and actual GUI/native plate binding. `B01-inputs.md` now retains exact source
   meshes/configs/transforms before normalization and invalidates the guarded
   Print revision; `B01-print-snapshot.md` retains full/effective/region settings.
   Source and actual native effective values remain distinct. Bind all remaining
   job IDs, plate changes, file/import error, software and qualified tool/scene
   dependencies before accepting worker/planner/verifier callbacks.
   `B02-input-placement.md` connects owned inputs to the native placement/error
   chain and enforces source plate-index agreement; actual GUI membership and
   original file-to-job association remain pending.
2. Verify new milestones on Linux with all budgets unchanged. The e8f40123 run
   36948380709 failed end/width replan and extended native void calculations at
   five seconds; original annotations are retained. B07-exact-union-reuse.md now
   reuses exact local geometry without changing precision/limits. Local material
   time is 57.688 -> 39.116 seconds; the native case is 16.780 -> 8.095 seconds.
   All old printed material/native expansions match with the original native seed;
   final CTest is 358/358 and six OFF/ZAA comparisons pass. Observe the new-source
   Linux result separately; local acceleration does not close the timeout gate.
   The inherited native-fill deadline failures at 84a709ef, 7f71cc98 and 946d3da7 are retained
   as historical evidence. `B07-constant-section-roof.md` is now verified by
   successful run 36847680702 at 2d8bebaa: selected native suites, STL CLI and
   application pass with the original 20-second timeout. Run 36853913690 at
   361ccdc8 also succeeds in native suites, STL CLI and application. Run
   36858083109 at 05f4a887 also succeeds in native suites, STL CLI and application.
   Run 36861891679 at 4e6cddd7 also succeeds in native suites, STL CLI and application.
   Run 36871942001 at 9a243279 now succeeds. Run 36878746418 at a165f0c3
   succeeds. The saved 18:31 UTC observation confirms d02d7544 material joins
   and 3d3f97a4 continuous runs succeeded; eb0e864a floor/body interface is in
   progress. New under-material deficit requires its own Linux verification. The A02 correction at d57e9325 also
   has successful native Linux run 36454696019.
3. Extend B03 nominal affine bounds to qualified error budgets, curved surfaces,
   ROI and tool access. Existing masks, whole-footprint checks and isolated CLI
   results remain diagnostic geometry, without export permission.
4. Complete B04 dense coverage and the adapter for the whole native body.
   `B04-partition.md` supplies an owned exact nominal body/cap partition with a
   common native interface. `B04-body.md` now derives every supported body layer
   from that reserved mesh, retaining source/effective configs and native roles.
   Extend hole/stepped native cases, bind worker/job ownership, reconstruct actual
   material and prove dense coverage or refuse; sparse bridge roles remain rejected.
   `B05-material.md` and `B05-body-material.md` now supply ordered constant-flux
   prefixes and all native body beads. Wider union domains, legal order
   and contact qualification remain open. `B05-coverage.md` now proves the whole
   flat 6x6 mm interior support plane through exact union coverage plus Clipper;
   `B06-material-transition.md` now bounds first-pass roof/gap/nominal volume
   against that actual prefix. `B06-material-integral.md` refines the nominal roof
   integral to a global mm3 interval width. `B06-pass-stack.md` selects source-bound
   prospective affine surfaces and whole-cell quotas within a total error budget.
   `B06-integral-strips.md` now allocates complete strip volume bounds from its
   owned continuous proof. Subdivide stepped support into qualified cells, derive
   finite fixed-width path/bead geometry from these bounds and solve
   seam/edge domains; prospective surfaces alone do not prove later cap support.
   `B07-first-hatch-beads.md` now derives actual first-centerline nominal roof
   gaps and bounded fixed-width amounts. `B07-material-union.md` now measures
   clipped declared nominal occupied union and multiplicity excess.
   `B07-material-fill.md` now separates covered/missing target and vertical spill
   with continuous actual roof bounds. `B07-material-deficit.md` now locates
   positive missing-volume lower witnesses over a complete exact XY grid.
   `B07-first-footprint.md` now constructs first-hatch amounts over the continuous
   roof of their complete finite width, with explicit domain provenance and
   off-axis ridge rejection. Rounded side-floor contact and target conformity
   still require qualification.
   `B07-remainder-hatch.md` constructs XY-clear intervals of one original line
   with measured positive nominal fill gain and exact preservation of the old
   current prefix. Extend to batch/global replanning and remaining vertical voids
   under occupied footprints; this does not resolve the complete native deficit.
   `B07-first-hatch-layer.md` now constructs every first source line with shared
   budgets and measures one fresh complete candidate together. Exact congruent
   packet unions preserve actual S/U/R, while C/M/spill remain measured rather
   than nominal quotas. The native two-line candidate still has 0.113705 mm3
   deficit; it does not certify sequential contact or a filled first pass.
   `B07-first-hatch-end-replan.md` now replaces that complete prospective list
   with longer finite butts under the unchanged source band, actual body/target
   and budgets. Native coverage rises by 0.0427173 mm3; deficit remains
   0.0709877 mm3. Do not append its old hypothetical cap to the new candidate;
   an actually laid partial prefix remains a separate construction obligation.
   `B07-first-hatch-width-replan.md` now replaces that candidate with narrower
   owned first-line widths and redistributed centres within the same band.
   Native S/R decrease by 0.0398967 mm3 while C remains approximately 0.181000
   mm3; M remains 0.0709876 mm3. Recompute complete strip owners and use owned
   line widths in all consumers; original generation policy remains provenance.
   Measured excessive coverage loss refuses replacement. Residual excess still
   needs qualification. `B07-first-contour.md` now owns four connected edges
   with explicit seam vertex/direction, whole-width roof proofs and joint corner
   union/fill. Its separate native candidate has M [0.102361,0.102772] mm3 and
   real positive spill. First-bead owners
   are optional only for protected contour edges; do not invent hatch owners.
   `B07-first-cap.md` now combines contour and interior owners in one fresh
   ordered ledger with joint measured fill, replacing the outer owners rather
   than duplicating them. The wider native target yields M [0.107167,0.107574]
   mm3, with real positive spill; do not compare gain across different ROIs.
   `B07-material-joins.md` now proves common whole boxes in the original D_lower
   at every requested local corner/interior end in analytical candidates. The
   unchanged native high corner has a positive witness, but full joining refuses
   the low corner: all nine relevant packets are shorter than twice their eroded
   finite-butt loss. `B07-material-runs.md` now separately certifies actual
   continuous runs with unchanged losses and all flux/sections across internal
   cuts. The same native cap has all six local joins and whole long-side lower
   coverage; the original per-event negative remains. This declared run model
   does not calibrate extrusion disturbances or physical bonding.
   `B06-cap-interface.md` now proves 3D lower body anchors and complete varying
   flat-floor nominal gap/overlap bounds from the original actual prefix. This
   retains the native 161 packets and their actual amounts; rounded shoulders
   and physical bonding remain unqualified.
   `B07-material-voids.md` now partitions original M into under-material and
   vertically clear volumes without inventing solid columns beneath cap roofs.
   The same native cap retains [.00793882,.00893881] mm3 under-material and
   [.0986354,.0992278] mm3 clear deficit with unchanged .001 mm3 precision.
   The original short work policy refuses; complete candidate budgets bound the
   measure. These nominal clear columns do not certify tool access or support.
   `B07-first-cap-end-replan.md` now retains all original contour/central packets
   and qualifies new finite end strips in one whole replacement ledger. Native
   missing volume falls to [.0955759,.0959988] mm3 and clear deficit to
   [.0863185,.0868956] mm3 with the same target/band/losses/.001 mm3 precision.
   All six run joins and 173 complete interface packets pass; actual
   under-material voids and repeated material remain. Shared original work/time
   budgets and all old negative cases remain.
   `B07-first-cap-width-replan.md` now retains every XYZ/gap, contour snapshot
   and end packet while narrowing 53 native central packets to .38 mm. S/R fall
   about .0063 mm3 and the native C/M/spill bounds stay unchanged; all six run
   joins, 173 interface packets and fresh void classification pass. Use actual
   owned packet widths, not original policy.width, after replacement. Wider
   analytic extended-shadow budget refusals still need bounded refinement.
   `B07-first-cap-material.md` now preserves the completed original body and
   selected actual cap fraction in one source-mapped prefix, excluding future
   material. One actual continuous run or the original independent-event lower
   union certifies local boxes; the native original second surface has a qualified
   local footprint. Full-ROI/future-cap support still refuses.
   `B07-next-cap-bead.md` now partitions whole Lower support across several
   actual runs/events and constructs one local actual later finite-width bead
   using certified nominal run-union roof planes. The native .4 mm footprint
   supports eight .38 mm packets with unchanged error limits and original later
   vertical policy. `B07-next-cap-material.md` now appends ordered batches with
   exact old rows, new owned packet origins and selected actual fronts. The
   existing surface/bead factories consume this material for a third pass;
   unlaid future support and backwards/stale batches refuse. Extend to complete
   later path/layer construction, normal thickness and contact qualification.
   Overlapping batches need sequential re-planning on the updated actual prefix.
   Add exact partial-body/partial-before continuation while retaining
   the original packet geometry and actual fraction.
   Qualify allowable repeated material and complete shoulder/seam volumes,
   construct/replan paths for both remaining 3D volumes and reconstruct later support;
   short-packet motion/flow limits remain unverified.
5. `B08-material-motion.md` now checks complete supplied rigid boxes and annuli
   continuously against the original actual journal: prior material is complete,
   current deposition grows with the nozzle, and future/pressure rows are absent.
   Whole-volume interval leaves accept; actual interior witnesses only refuse.
   The original native later packet and connector pass for high simulation tools;
   a low rigid box refuses on that current row. Compose the complete measured head
   and static scene, qualify exact deposition contacts and applicability, and bind
   whole-job ordered entry/exit/travel/parking, independent final-byte replay and
   guarded export. `B08-scene-material.md` now composes all captured head/static
   pairs with the actual material proof for the same original event and shared
   budgets; full potential Upper tops constrain omitted-head coverage. The actual
   native next packet passes with a Z=0 annulus and six head boxes; a recaptured
   low rigid box refuses that current row. Original-index Travel and the complete
   64-box-plus-tip maximum pass. Qualification/contact/job remain pending.
   Continue the normative backlog without closing P2 from bounded
   modules. The saved 01:23 UTC Linux observation confirms 2c446685 succeeded;
   e8f40123 failed the retained five-second calculations. The saved 02:23 UTC
   observation has a258877a in progress and 0dcbd642 pending; new source remains
   unverified on Linux.

Independent review of A03–A08 remains required for Gate A closure. Operator
measurements, machine/material confirmation and physical coupon runs remain
separate pending work; do not substitute synthetic evidence for them.

A07 operator evidence remains NOT_RUN. SYS-08 full-cycle performance is unmeasured.
Normative dependencies remain in `../nonplanar/backlog.json`; no large UI is next.

| Task | Restricted change scope / deliverable | Positive and negative evidence |
|---|---|---|
| A04 (implemented; review pending) | Native annular-tip/plane and translating asymmetric box/box queries, interval bounds and limits; see `A04.md`. | GEO-01/02/03/04/05/07/08: 12 native cases, including 300 independent slab-oracle scenes. No sampled-only PASS. |
| A05 (implemented; review pending) | Small native transition experiment and ADR deciding the body reservation hook. No GUI. | INT-01/02/03 and VOL-01: accepted dense support example and rejected stair gap; independent integral of reserved/body/cap volume; preserve an unsupported transition as negative fixture. Scope of valid geometry explicitly bounded. |
| A06 (bounded experiment; review pending) | Tiny ordered IR adapter to native `GCodeWriter` plus a separately implemented limited parser. No use of `_extrude` ZAA branch. | ORC-06/07, GCD-01: nonzero absolute Z across nominal layer boundaries; k=1 and k≠1 E; mutate XYZ/E, modal state, rounding and unknown command. Include plate offsets and writer's omission of unchanged Z. |
| A07 (simulation implemented; operator evidence pending) | Versioned simulation scene/profile contract and operator measurement worksheet. Existing measurement template remains unconfirmed. | PRF-01/06, ORC-38: finite tip/full head, missing dimensions and out-of-coverage scene reject; conservative moving-part envelope with stated domain. Real dimensions/photos must come from the operator. |
| A08 (audit/benchmark recorded; independent review pending) | Critical review against implementation/analytical evidence, CPU/memory benchmark and revised effort estimate. | SYS-08, DOC-01: count executed tests, retain negative cases and useful positive paths; identify unresolved hooks, NOT_RUN and numerical/physical limits. No Gate A completion from preview/helper tests. |

Only one owner changes the IR/numerical contract. Any contract change needs an
ADR, consumers/migration review and corresponding test changes before parallel
work on dependent modules. Printer operation is never an agent task.

A07 first machine: **Snapmaker U1**, selected by the user. See
`A07-snapmaker-u1.md` for the unconfirmed configuration and measurement needs.
