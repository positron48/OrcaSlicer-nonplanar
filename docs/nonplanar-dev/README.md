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
selected CTest executes 119/119; all ten CLI cases pass expected outcomes. The six
B02-snapshot OFF/ZAA comparisons pass. Hard memory/I/O containment, model transforms,
the full job error budget, whole-job snapshots and a successful hybrid pipeline
remain unimplemented. Gate A still
requires independent review; digital coupon preparation is not physical proof.
