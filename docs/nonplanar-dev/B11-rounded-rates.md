# B11 native decimal rate retuning

Date: 2026-10-03. Branch `feature/nonplanar-bootstrap`. Parent `6f2e7309`.
Commit: `SELF`, resolved by `git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B11-rounded-rates.md`.
ADR-0098; critical author review in `B11-rounded-rates-review.md`.

The full-stop candidate producer now calculates rate ceilings from the native
formatter's actual decimal XYZ/E. It preserves original geometry, E conversion,
pressure, journal/order, margins and policy limits while lowering only M204/F
as needed. The independent exact final-byte verifier is unchanged. Original100
later analysis now passes rates with M20485.530744; the old85.530745 command
still fails Z50. Full B01–B15 remains active, fixed17 remains4 PASS/RUN and13
UNKNOWN/NOT_RUN, and guarded export remains BLOCK.

Evidence and exact commands/exit codes are frozen in
`evidence/B11-rounded-rates/source-manifest.json` and `commands.json`.
Raw evidence: `build/nonplanar-evidence/B11-rounded-rates`.
Git source/dependency, binary, archive and lossless verification is in
`evidence/B11-rounded-rates/delivery/`.

Final focused verification:9 cases/2480 assertions, zero failures. CTest486/486,
zero skips/failures, 155.782s;242 nonplanar and244 fff cases,447 Nonplanar
labels and39 other cases. Eighteen independent oracles, thirteen actual CLI
scenarios and six strict OFF/ZAA pairs PASS.41/51 parent files remain exact;
nine change only cumulative work and one software-bound report. Original2098
candidate SHA remains `c330363d909d5907a52b151a7ca499d1ce72681cc0185edd6f9c312bef12686b`.
The original100-policy1756-record XYZ/E/order/barriers are exact; the actual
separate rate auditor passes the retuned bytes and refuses both historical and
restored unsafe acceleration bytes. Fresh isolated bundle probes PASS.
Targeted tests include the compiled test-first red, mirrored Cartesian/CoreXY
positive, coarse XYZ/E axis/drive/filament/Q corrections, restoration of unsafe
ceilings, cross-section refusal, exact work thresholds and the existing complete
candidate/later-controller/child/capture/refusal cases. Separate Python Decimal
and native auditor evidence preserves old XYZ/E and both unsafe-byte negatives.

Native consumers, actual CLI, stock OFF/ZAA, source/package, compiled inventory,
parent provenance and isolated bundle probes must qualify the final binaries.
macOS ARM64, AppleClang21 and Release Ninja/CMake are the local environment.
GUI and physical tests are NOT_RUN; prior GUI evidence remains historical.
Linux new-head CI is observed separately after push; Windows and independent
safety review remain pending. Known U1 configuration remains standard head and
0.4mm nozzle; there is no firmware/material-brand questionnaire.

Next: complete filled later layers/cap and qualified seams/contact/head/connector
routes/order/whole-job geometry, then remaining source/import/3MF/domain UI,
hard resources/process containment, software/all17/physical qualification and
atomic publication. This rate fix does not complete those requirements.
