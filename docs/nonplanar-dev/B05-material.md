# B05 ordered deposited-material ledger

Invariant: one owned, revision-bound MotionEvent sequence produces distinct nominal,
upper and lower material views at an explicit within-event prefix. Completed
previous beads remain; only the traversed current fraction exists. Travel and
retraction/restoration create no bead. Contact IDs do not remove any geometry.
ADR-0030 fixes the vertical-section constant-flux contract and internal schema 1.

Seven new native cases / 4466 assertions pass with NoAssertions. Independent
long-double angled-section tests check nominal point membership and inner/outer
inclusion away from uncertain boundaries. Rectangle and rounded sections retain
curved shoulders; sloped nozzle Z does not multiply volume by a 3D slope factor.
A varying vertical gap changes section width at constant area V/XY_length, as
required by G1 linear E interpolation. Every width must fit the declared range;
MotionEvent width anchors the midpoint gap. Prefix commanded volume grows as V*t.
It is cumulative delivered amount, not the measure of overlapping bead unions.

A targeted test first exposed the old inconsistent fixed-width varying-gap law:
at 40% of a 3 mm3 event it reconstructed .96 mm3 instead of 1.2. Preserve its exit
42 and failure output. Corrected positive tests observe the actual changing width,
descending gap, partial growth and absent future material. State/order/volume/
width/gap/discontinuity mutations, unsupported pure vertical deposition, repeated
IDs, cancel/stale/deadline, callback destruction and unsupported arithmetic all
reject without accepted state. Prior negative tests were preserved.

Material envelopes use independent XY/Z outer growth and inner loss plus a stated
numerical coordinate error. Lipschitz section bounds cover longitudinal variation;
an exhausted inner section gives no guaranteed support. These are declared model
set assumptions, not physical qualification. Point boundaries and unsupported
numeric/work contexts return Unknown. Varying-gap lofts need not be convex, so
point or vertex membership cannot prove whole-footprint coverage or tool CCD.

A struct/hashlib Python vector independently checks every event payload, section,
derived intervals, model/revision/source, domain-separated record hash chain and
prefix identity. The synthetic vector tests encoding only. Prefix identities bind
completed count, progress, cumulative amount and the parent ledger. Fixed aggregate
record/work limits, bounded per-record canonical encodings and cooperative checks
prevent partial publication; hard RSS/deadline containment remains open.

Apple Clang 21 macOS ARM64 Release app and both native test targets build with
exit 0. Selected CTest discovers and executes 238/238 with no disabled/skipped
cases. Six fresh OFF/ZAA G-code/modal comparisons match pinned stock, allowing
only the existing normalization and additive nptop_mode=off setting. No original
specification, source fixture or golden was modified. Source/binary hashes,
commands/exit metadata, XML, discovery and failure excerpts are preserved under
`evidence/B05-material/`; raw logs are under `build/nonplanar-evidence/B05-material/`.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/nonplanar/Release/nonplanar_tests.app/Contents/MacOS/nonplanar_tests '[MaterialModel]' --warn NoAssertions
python3 scripts/nonplanar/material_fingerprint_oracle.py
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B05-material/ctest --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B05-material-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B05-material-baselines --allow-nptop-off-default
```

Author review caught and corrected the in-event flow law, fixed unbounded source
string copying and moved captured records instead of copying them a second time.
Review checked ownership before callbacks, pressure continuity, geometry retention,
interval rounding/section derivatives and complete identity. This is not the
required independent critical review, which remains pending. Linux new revision
is not verified; Windows and physical execution are NOT_RUN.

This foundational ledger has no native body adapter yet and provides no local
contact qualification, first-contact localization, collision/support/coverage
approval or export route. MAT-02/MAT-04 tags prove geometry retention only; complete
normative cases remain open. Next bind real owned native body segments and float
Flow to this material model, then prove dense under-cap coverage/refusal and seam.
The full B01–B15 objective remains active and public guarded export stays blocked.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B05-material.md`.
Diff: `git show <resolved-implementation-revision>`.
