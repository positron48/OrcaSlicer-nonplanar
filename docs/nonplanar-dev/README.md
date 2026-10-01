# Nonplanar development evidence

The immutable incoming specification is in `../nonplanar/`; its checksums and
planned statuses describe the original delivery, not current implementation.
Current work is recorded here separately so package validation retains its
original meaning. No source snapshots or golden outputs have been regenerated.

## Bootstrap scope

Base: `8500fcdccaa10b5099ac20d252af3a7c560046f1` (v2.4.2).
Branch: `feature/nonplanar-bootstrap`, published to
[the user fork](https://github.com/positron48/OrcaSlicer-nonplanar/tree/feature/nonplanar-bootstrap).
Bootstrap: `1ad7bdad`; A04: `93192ca7`. `status.json` is the current checkpoint; its `SELF`
revision resolves with the included Git command.

## Current checkpoint — 2026-10-01

The current implementation boundary is exact pre-apply native source ownership
and revision invalidation in `B01-inputs.md`. Raw settings, meshes, transforms,
overrides, annotations and source metadata are owned before Orca normalization;
changes hidden by approximate native comparisons still invalidate guarded output.
Print settings identity is schema 2, binding source fingerprint/revision. Six new
cases/90 assertions and all 54 B01 cases/4591 assertions pass with NoAssertions;
selected CTest executed 221/221 without skips at that revision.
`B02-input-placement.md` now connects owned source inputs to the original STL and
native geometry placement, independent of live GUI Model lifetime. Two new cases/
62 assertions and all 46 B02 cases/791 assertions pass; current selected CTest
executes 223/223 without skips, with six fresh OFF/ZAA comparisons passing.
`B04-partition.md` adds a common exact body/cap interface with preserved walls,
hole and stepped-interface fixtures: five cases/4881 assertions pass. Current
selected CTest executes 228/228; six fresh OFF/ZAA comparisons pass.
`B04-body.md` now derives all supported body layers through native Print with
owned settings and path semantics: three cases/2965 assertions pass. Final selected
CTest executes 231/231; six fresh OFF/ZAA comparisons pass. Sparse native bridge
roles and generated helpers reject; this is nominal data, not support approval.

`B05-material.md` adds owned ordered material and within-G1 constant-flux prefixes:
seven cases/4466 assertions pass. `B05-body-material.md` binds every actual native
body segment and reconciles float Flow: three cases/46118 assertions pass. Selected
CTest executes 241/241 at that checkpoint. `B05-coverage.md` adds continuous union
coverage and an actual 6x6 mm interior support plane confirmed by an independent
Clipper oracle: four cases/50 assertions pass. GMP's pinned Apple ARM64 ABI fix
removes reserved-x18 assembly; 197 dependency programs and 100 randomized native
repetitions pass. Final selected CTest executes 245/245 and six fresh corrected-
library OFF/ZAA comparisons pass. Full seam/edge domains, legal order and local
contact qualification remain open.

`B06-material-transition.md` adds first-pass affine feasibility against one actual
material prefix, with separate continuous lower support and upper roof/gap bounds.
Five new cases/84 assertions pass, including actual native cap interior, blocked
edge and an independent step-volume integral. Nominal volume remains a diagnostic
interval; thickness selection, refined integration and volumetric seam are open.
Final selected CTest executes 250/250 and six fresh OFF/ZAA comparisons pass.
`B06-material-integral.md` now refines volume above the highest actual rounded
roof to an explicit total interval width, with independent circular/step integrals
and a Clipper/monotone-Z oracle for the native floor. Four new cases/8293 assertions
pass. Final selected CTest executes 254/254 with six fresh OFF/ZAA comparisons.
Selected bead volume/E, thickness and complete seam/path allocation remain open.

`B06-pass-stack.md` now selects source-bound prospective affine surfaces and
whole-cell quotas, preserving the final target and checking actual first-gap,
later vertical/normal spacing and total volume error. Four new cases/128
assertions pass, including a physical-frame translation and a native reserved
interior. Current selected CTest executes 258/258; six fresh OFF/ZAA comparisons
pass. Subsequent actual cap support, finite paths and volumetric seam remain open.

`B06-integral-strips.md` retains an opaque continuous roof proof and derives
complete X/Y strip volume bounds without repeating material queries. Three new
cases/119 assertions and six added selected-first-pass assertions pass. Current
selected CTest executes 261/261; six fresh OFF/ZAA comparisons pass. Finite bead
allocation and seam corrections remain open; volume cells do not authorize E.

`B07-first-hatch-beads.md` now reconstructs the continuous first-centerline
nominal roof/gap from actual laid prefixes and bounds constant-flux amounts.
Three geometric cases/87 assertions and one native case/52999 assertions pass;
all 14 native first lines produce 17624 packets under explicit gap/width/amount
errors. Selected CTest executes 276/276 and six fresh OFF/ZAA comparisons pass.
Finite-width contact, filled unions/overlap, perimeter/seam, later deposited
support and full motion/order remain open; short-packet feasibility is unverified.

`B07-material-union.md` now bounds clipped nominal occupied union, individually
clipped summed amounts and multiplicity excess, preserving actual prefixes and
finite gaps. Seven geometric cases/170 assertions and one native case/37
assertions pass. All 14 first lines / 2109 declared packets yield occupied volume
1.3122–1.3222 mm3 versus summed amount 1.3957 mm3, at 0.01 mm3 interval precision.
At that checkpoint selected CTest executes 284/284; six fresh OFF/ZAA comparisons pass.

`B07-material-fill.md` now separates covered and missing target volumes from
below-roof and above-surface spill, using protected actual roof/union sources.
Five analytical cases/126 assertions and the extended native case/54 assertions
pass. At that checkpoint selected CTest executes 289/289; six fresh OFF/ZAA comparisons pass.
The native first candidates have approximately 1.27-1.29 mm3 proven deficit and
0.015-0.019 mm3 outside volume. Localized fill resolution, finite-width contact,
seam and later support remain open.
`B07-material-deficit.md` now proves missing-volume lower witnesses over a
complete XY grid. Three analytical cases/118 assertions and the extended native
case/64 assertions pass. The four native quarters jointly require at least
0.98329 mm3 more material, without asserting a complete missing-shape map or
repair approval. At that checkpoint selected CTest executes 292/292 and six fresh OFF/ZAA
comparisons pass. Constructing and qualifying the repair paths remains open.

`B07-first-footprint.md` now constructs bounded first-hatch amounts against the
continuous roof across the complete finite width and explicitly distinguishes
them from centerline diagnostics. Four analytical cases/74 assertions and one
new actual native case/18 assertions pass, including an off-axis ridge refusal
and a positive native flat-core path. At that checkpoint selected CTest executes 297/297;
six fresh OFF/ZAA comparisons pass. Rounded side-floor contact, target conformity
and actual deficit repair remain open.

`B07-remainder-hatch.md` constructs remaining first-hatch intervals separated
from actual D_upper projections, re-proves finite widths and measures positive
nominal target gain. Three analytical cases/178 assertions and the extended
native case/29 assertions pass. Native added coverage is at least 0.06858 mm3;
remaining deficit is still approximately 0.131 mm3. At that checkpoint selected CTest
executes 300/300 and six OFF/ZAA comparisons pass. Full 3D remainder, global
replanning, seam/contact and order remain pending.

`B07-stadium-depth-bounds.md` intersects rounded nominal depth with its exact
nonnegative domain. Two analytical cases/26 assertions pass: current/full
material stays inside an affine target within two cells, while a real raised
stadium retains positive spill. At that checkpoint material/body suites execute 57/14 cases
and selected CTest executes 302/302; six fresh OFF/ZAA comparisons pass. Native
fill uses 40960 cells / 865083 work locally, a small reduction that does not prove
the inherited Linux deadline is resolved. Limits and public export are unchanged.

`B07-constant-section-roof.md` evaluates nominal constant-Z/gap sections directly
when the full polygon lies strictly between actual finite butts. Rounded shoulders
and all budgets remain unchanged; other domains retain exact XY clipping. Four
analytical cases/157 assertions pass, including real spill, reverse diagonal
current support, outside rectangular neighbours and interrupted/mutated inputs.
At that checkpoint material/body suites execute 61/14 cases and selected CTest executes
306/306; six fresh OFF/ZAA comparisons pass. Native fill uses the same 40960 cells / 865083 work and takes 16.01
seconds locally. Linux repair remains pending; discarded index trials are only
diagnostic evidence.

`B07-first-hatch-layer.md` constructs every first source line under shared
budgets and measures a fresh complete prospective ledger together. Two first-
layer analytical cases/145 assertions and one exact congruent-union case/56
assertions pass. The unchanged native wedge produces both lines/122 packets;
the extended case passes 46 assertions, independently enclosing commanded
S/U/R and keeping section targets distinct. Nominal C is 0.138283 mm3, M
0.113705 mm3 and R 0.107875 mm3, with 63 cells/17802 work. At that checkpoint material/body
suites execute 64/14 cases; selected CTest executes 309/309 and six fresh OFF/ZAA
comparisons pass. Failed integration trials remain diagnostic evidence. Complete
3D repair/contact/seam, curved/later support and order/export remain pending.

`B07-first-hatch-end-replan.md` replaces the complete prospective first list
with longer finite butts, preserving the original band, transverse centres,
actual body and target. Three analytical cases/183 assertions and the extended
native case/64 assertions pass. Native C rises to 0.181000 mm3 and M drops to
0.0709877 mm3, with gain at least approximately 0.0427173 mm3. A ridge only in
the new domain rejects the whole replan; future material remains absent. The
new owned first-pass extent provenance increments the internal affine hatch
contract 2 -> 3, with no persisted formats affected. Current material/body
suites execute 67/14 cases, selected CTest executes 312/312 and six fresh OFF/ZAA
comparisons pass. Remaining 3D repair, perimeter/contact/seam and actual later
support are still required; this is prospective geometry, not printed material.

The complete B01–B15 objective remains active and `gate-b-plan.md` records the
full acceptance scope. Source/file/plate/job/profile/scene binding, complete
compatibility, dense coverage, material replay, ordering and guarded hybrid
export remain open. `next-tasks.md` lists the dependency order. Independent review
and physical qualification remain pending. U1 standard head and nominal 0.4 mm
nozzle are user-declared in `B15-u1-declared-setup.md`, without measurement claims.

Prior Linux native runs at d57e9325, 7df1d765, e7a46961, 23f00bfd,
b35408ce, a083f2df, 63779b4f and 8bcbbd45 succeeded. Run 36809462993 confirms
8bcbbd45 native tests and application. The latest saved snapshot records successful e2030e15 runs
36812147275 and 36819847014 and successful 4b3da47d run 36820170446;
run 36826154741 at 84a709ef failed the native fill case on its unchanged
20-second deadline. Run 36828766748 at 7f454ba5 was cancelled and run
36831523726 at 7f71cc98 also failed that deadline (38309 cells / 814580 work).
Run 36842571249 at 946d3da7 also failed the native fill deadline
(40862 cells / 857455 work), with check annotations retained in the new first-
hatch-layer archive. Run 36847680702 at 2d8bebaa now succeeds in selected
native suites, native STL CLI and application build, retaining the original
20-second fill limit. Run 36853913690 at 361ccdc8 is building native tests in
the saved 11:37 UTC observation. This new end replan is not yet Linux-verified.
Both failed check annotations are archived.
Windows remains NOT_RUN.

The continuation sections below preserve historical counts and next steps;
`status.json` and this checkpoint describe the current state.

Follow `../nonplanar/prompts/01_bootstrap.md`: A01–A03, then concrete A04–A08
tasks. Native macOS stock/fork builds, 37 FFF tests, seven analytical C++ tests
and six OFF/ZAA differential cases passed. See bootstrap-report.md. Gate A as
a whole is IN_PROGRESS: A08, operator evidence and independent critical review remain ahead.

## A04 continuation

`A04.md` records the published continuation: continuous primitive
clearance queries, finite tip/full gradient, fixed asymmetric box sweeps and
fail-closed numerical/resource limits. Twelve new native cases plus seven A03
cases pass, including 300 independent slab-oracle scenes; six fresh OFF/ZAA
captures match stock. The ordinary 37-case FFF CTest baseline passes, while a
separate NoAssertions run diagnoses four empty sections in two unchanged
upstream cases. That stricter run is retained as a failure, not hidden.
Evidence and the source fingerprint are under `evidence/A04/`.
No new export path exists.

## A05 continuation

`A05.md` and ADR-0007 record native body reservation, reconstruction of the flat
core of actual planar paths and bounded affine first-pass gap/volume checks.
Nine new cases pass; complete CTest executes 66 cases without skips. Six final
OFF/ZAA captures match the unchanged stock baseline. The stricter randomized
FFF run passes 45/47 cases and retains the two existing empty-section failures.

The differential gate found an uninitialized object-label ID in inherited
G-code code. `091c6461` fixes it separately; `A02-object-labels.md` records the
negative test and diagnostics. Failed runs and source fingerprints are preserved
under `evidence/A05/`. Next: A06 serialization/parser spike. No production hybrid
export, complete transition solver or physical qualification is claimed.

Allowed changes are defined in each milestone report: isolated Nonplanar
primitives, native integration tests and evidence. The shared stock-path label
correction is explicitly documented; stock slicing/ZAA algorithms are unchanged.

Invariants: exact upstream revision; native ARM64 tests actually execute;
absolute physical XYZ never enter relative ZAA offsets; deposition volume,
filament retraction and nominal/upper/lower material are distinct.

Acceptance IDs: A01 ORC-01/ORC-31; A02 ORC-02/ORC-03; A03
ORC-05/VOL-01/VOL-02. Package helper tests do not satisfy these IDs.

Linux x86_64, printer measurements and physical tests remain NOT_RUN until
their actual environment/operator evidence is available. Stock application
launches must use an explicitly isolated data directory; never use user presets.

## A06 continuation

`A06.md` and ADR-0008 record a bounded native GCodeWriter adapter and independent
STL-only text replay. Six new cases pass (1342 assertions, including 100 shifted
round-trips); selected fresh native CTest executes 72/72 cases with no skips.
The implementation revision resolves via the command in `A06.md`. No production export or safety
approval is introduced. A07/A08 remain next; detailed failures and limitations
are retained under `evidence/A06/`.

## A07 continuation

`A07.md` and ADR-0009 record a versioned synthetic scene/profile applicability
contract and an unconfirmed operator worksheet. Six new cases pass; selected
fresh native CTest executes 78/78 cases without skips. Confirmation claims do
not qualify synthetic geometry. Full coverage of declared envelopes does not
imply collision freedom or export permission. Next is A08 review/benchmarks;
operator measurements and physical qualification remain NOT_RUN.

## A08 continuation

`A08.md` records a source-based author audit, one fixed native feed-rounding
defect, a 10000-move serialization/replay stress case and five-repeat CPU/RSS
measurements. Selected CTest executes 80/80 cases. The 25 measured processes
exercise bounded native modules; SYS-08's full 200k pipeline remains NOT_RUN.
`A08-review.md` records findings and `A08-estimate.md` revises remaining effort.
Gate A stays open for analytical sphere evidence, a prepared coupon/protocol
and independent review. Operator data and physical qualification remain absent.

## B01/B02 continuation

`status.json` is the current checkpoint. `B01.md` records a bounded resolved
mode preflight and native/CLI/archive output blocks; the full compatibility
registry is still pending. `B02-parser.md`, `B02-mesh.md` and `B02-snapshot.md`
record bounded STL metadata, parsed geometry checks and immutable source-byte
provenance. `B02-error.md` adds bounded decimal conversion error; `B02-worker.md`
adds supervised native analysis bound to source bytes and revision. `B02-file.md`
adds bounded source-file capture and the actual native diagnostic CLI. Latest
selected CTest executes 127/127 after the native placement-bound addition
(`B02-placement.md`, ADR-0012) and callback arithmetic fix (`B02-callback.md`);
all ten CLI cases pass expected outcomes. The six
B02-snapshot OFF/ZAA comparisons pass. Hard memory/I/O containment, native Model/plate transform provenance,
the full job error budget, whole-job snapshots and a successful hybrid pipeline
remain unimplemented. Gate A still
requires independent review; digital coupon preparation is not physical proof.

`A01-gui.md` records the fresh full macOS build, six OFF/ZAA comparisons and an
isolated native GUI import/slice/preview/quit run at `1e0b5b98`. Actual sandbox
write/network probes pass. Stock coexistence and hybrid GUI remain unverified.

`B01-hooks.md` adds the explicit 17-field native custom-code registry, sparse
source checks and guarded plate-action invalidation. Thirteen B01 cases pass
(286 assertions); current selected CTest executes 132/132 and six fresh OFF/ZAA
comparisons pass. Full compatibility, whole-job snapshots and hybrid success
remain pending.

`B01-transforms.md` adds eleven typed neutral-value rules for unqualified flow
and geometry compensators. Current selected CTest executes 135/135; sixteen B01
cases/377 assertions and six fresh OFF/ZAA comparisons pass. Numeric edge cases
are rejected before lossy or invalid native serialization.

`A01-coexistence.md` records simultaneous isolated native processes on
2026-09-28. Interactive stock GUI coexistence remains unverified: wrapper
automation failed, and a disposable app copy was rejected during launch with
a failing signature check. All test processes were stopped; the installed
stock executable stayed unchanged. No OS protection bypass was attempted.

`B01-snapshot.md` and ADR-0013 add owned native resolved settings, including
generic enum dictionaries. Nineteen B01 cases/2845 assertions, 138/138 selected
CTest cases and six fresh OFF/ZAA comparisons pass. Whole-job provenance and
fingerprints remain pending.

`B01-discrete.md` replaces string equivalence with native type/value rules for
ten compatibility fields and requires a scalar-string mode. Twenty-three B01
cases/2906 assertions, 142/142 selected CTest and six fresh OFF/ZAA comparisons
pass; full compatibility remains open.

`B01-layering.md` rejects custom source layer-height profiles and height-range
overrides while preserving OFF inputs. Twenty-six B01 cases/2940 assertions,
145/145 selected CTest and six fresh OFF/ZAA comparisons pass.

`B02-centering.md` and ADR-0014 bind captured native volume-local geometry to
its STL source after centering, including an independent rounding-error oracle.
Four cases/70 assertions, 149/149 selected CTest and six fresh OFF/ZAA comparisons
pass. The remaining transform chain and whole-job integration are still open.

`B02-model.md` and ADR-0015 add sequential native Model transform capture and
error propagation. Fourteen placement/centering/model cases/227 assertions,
153/153 selected CTest and six fresh OFF/ZAA comparisons pass. GUI plate binding,
whole-job/worker integration and the hybrid pipeline remain open.
