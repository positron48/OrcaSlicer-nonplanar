# Nonplanar development evidence

The immutable incoming specification is in `../nonplanar/`; its checksums and
planned statuses describe the original delivery, not current implementation.
Current work is recorded here separately so package validation retains its
original meaning. No source snapshots or golden outputs have been regenerated.

## Bootstrap scope

Base: `8500fcdccaa10b5099ac20d252af3a7c560046f1` (v2.4.2).
Branch: `feature/nonplanar-bootstrap`. Bootstrap is committed locally; no remote
fork has been published. `status.json` is the current checkpoint; its `SELF`
revision resolves with the included Git command.

Follow `../nonplanar/prompts/01_bootstrap.md`: A01–A03, then concrete A04–A08
tasks. Native macOS stock/fork builds, 37 FFF tests, seven analytical C++ tests
and six OFF/ZAA differential cases passed. See bootstrap-report.md. Gate A as
a whole is IN_PROGRESS: A05-A08 and independent critical review remain ahead.

## A04 continuation

`A04.md` records the locally committed continuation: continuous primitive
clearance queries, finite tip/full gradient, fixed asymmetric box sweeps and
fail-closed numerical/resource limits. Twelve new native cases plus seven A03
cases pass, including 300 independent slab-oracle scenes; six fresh OFF/ZAA
captures match stock. The ordinary 37-case FFF CTest baseline passes, while a
separate NoAssertions run diagnoses four empty sections in two unchanged
upstream cases. That stricter run is retained as a failure, not hidden.
Evidence and the source fingerprint are under `evidence/A04/`.
Next: A05 transition/body reservation experiment. No new export path exists.

Allowed changes: documentation/evidence, isolated build/test tooling, then a
small typed IR module and its analytical tests after studying native types.
Do not modify stock slicing or ZAA behavior in the bootstrap patch.

Invariants: exact upstream revision; native ARM64 tests actually execute;
absolute physical XYZ never enter relative ZAA offsets; deposition volume,
filament retraction and nominal/upper/lower material are distinct.

Acceptance IDs: A01 ORC-01/ORC-31; A02 ORC-02/ORC-03; A03
ORC-05/VOL-01/VOL-02. Package helper tests do not satisfy these IDs.

Linux x86_64, printer measurements and physical tests remain NOT_RUN until
their actual environment/operator evidence is available. Stock application
launches must use an explicitly isolated data directory; never use user presets.
