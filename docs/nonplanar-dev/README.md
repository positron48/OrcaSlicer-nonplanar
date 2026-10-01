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

The complete B01–B15 objective remains active and `gate-b-plan.md` records the
full acceptance scope. Source/file/plate/job/profile/scene binding, complete
compatibility, dense coverage, material replay, ordering and guarded hybrid
export remain open. `next-tasks.md` lists the dependency order. Independent review
and physical qualification remain pending. U1 standard head and nominal 0.4 mm
nozzle are user-declared in `B15-u1-declared-setup.md`, without measurement claims.

Prior Linux runs 36454696019 at d57e9325 and 36758101513 at 7df1d765 succeeded.
Runs 36767506583 at e7a46961 and 36786738740 at 23f00bfd succeeded. In the saved
snapshot, 36791501134 at b35408ce succeeded, 36793524250 at 22478ec6 is cancelled,
36796275758 at a083f2df is in_progress, and 36800464013 at 3310bde1 is pending.
These records do not prove Linux execution of this strip-allocation patch.
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
