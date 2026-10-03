# B13: protected compiled build input inventory

The library now supplies its own Software resource for native guarded jobs.
`CompiledInputs` accepts six declared resource roles and inserts the exact
inventory compiled into libslic3r; a supplied Software resource is rejected.
The immutable snapshot's value constructor is private. The existing job,
candidate and report fingerprints bind its bytes and SHA-256. Declared diagnostic
jobs retain their version-1 format and have no protected software snapshot.

CMake observes 18,285 source/resource/build files by content on every build,
with sorted relative paths and declared compiler/configuration/target context.
Its 2,433,680-byte canonical inventory has SHA-256
`b4243936d6de449183e4affeba0f35f3cfe7fc4ef44af52d71071cbda7681155`.
Generated literals preserve UTF-8 and remain bounded; MSVC uses `/utf-8`.
Unchanged builds run the generator without recompiling or relinking.

Native body/lineage/departure/report fixtures call the protected mode. The
independent oracle recomputes every observed file digest and verifies the exact
Software resource in all three native reports. Other resources remain declared
and unqualified. No imported JSON is used to create the protected snapshot.
Original resource bounds, one second capture deadline, OFF bypass, cancellation,
owner/attempt/phase and final native freshness checks remain.

Tests exposed both stale objects and stale linkage under GNU Make 3.81's coarse
timestamps. The final implementation obtains the inventory object's exact path
from CMake and invalidates that object and producer/transitive consumer outputs
on inventory changes in Make builds. Independent targets remain untouched; every
removal must remain inside the physical build tree. Missing leaf paths and
symlinked ancestors are handled, and outside-tree outputs fail before deletion.
The libslic3r/libnest2d static cycle is retained. Ninja uses generated-header
dependencies. All intermediate failures and the controlled build interruption
are retained in the lossless archive.

macOS ARM64 / Apple Clang 21.0.0 / Release verification:

- All four targets built successfully; final build exit 0 / 17.731 seconds.
  Unchanged repeat exit 0 / 13.049 seconds, no compilation or linking.
- Eight isolated generator tests pass, including content edits preserving mtime,
  additions/removals, direct/static/transitive/cyclic consumers, exact UTF-8
  chunk reconstruction, invalid inventories and external-output refusal.
- C++ focus: 10 cases / 230 assertions; selected CTest: 453 / 453, zero failures
  or skips, 110.587 seconds. Existing geometry/contact/support/refusal tests run.
- Nine independent identity oracle commands pass: complete inventory plus three
  native dependency bindings, context, candidate manifest and six reports.
- Six fresh strict OFF/ZAA comparisons pass the existing stock comparator;
  only its original timestamp normalization/off-default allowance applies.
- Source/package and workflow YAML checks pass. Original specification,
  real presets, printer, firmware and cloud remain unchanged.

Fifty of 51 parent native candidate files remain byte-exact. Only the report
bound to the new Software resource changes. Its actual motion bytes, source,
initial pose, policies and journal remain exact. The final supported-deposition
candidate is still 2205 records / 128014 bytes, SHA-256
`b8eb15681127c1a8db57ce0aa257326763f16832b82fdafd13ed091c4bd98ef8`.
No old hash is normalized and no old certificate is reused.

This inventory does not prove every reused object's compilation provenance,
complete effective flags, installed dependency/archive identity, executable
linkage, loaded plugins or runtime resource copies. A source edit preserving
mtime can still leave an unrelated object inconsistent with the inventory.
Complete `software_identity` remains UNKNOWN/NOT_RUN. The fixed registry retains
4 PASS/RUN + 13 UNKNOWN/NOT_RUN; export BLOCK and full B01–B15 IN_PROGRESS.
The analytical CLI matrix was not rerun in this change; prior evidence remains
historical. Linux new source, Windows, full GUI, independent review and physical
qualification are NOT_RUN/unconfirmed here. Standard U1 head / 0.4 mm nozzle
are known; firmware/material-brand questions do not block software implementation.

Next work must close complete source/resource-to-plan binding and software
provenance, qualified contact/seams/full cap fill, whole-job geometry/order and
GUI/publication before changing any outstanding mandatory check. ADR-0088 and
the separate author audit record the contract and remaining risks.
