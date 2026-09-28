# B01 neutral native extrusion multipliers

ADR-0025 adds sixteen exact-native rules to the existing neutral-transform
registry. The initial preflight accepted filament/print/role/bridge/brim flow
changes without a calibrated delivered-volume contract, even though GCode.cpp,
PrintRegion.cpp and Extruder.cpp apply them to volume/extrusion. The registry
now requires 1.0 in every present native scalar/vector entry on both source and
resolved paths. Missing/wrong types, nonneutral values including one ULP above
1.0, NaN/infinity and empty/mixed filament vectors are rejected without mutation.
All settings remain owned by the snapshot; OFF behavior is unchanged.

Two predefined regression cases initially record 231 failed checks (exit 42).
They cover all sixteen factors, source/resolved checks, exact fingerprint
preservation, OFF behavior and a real native object override. With the rules
implemented, all 39 B01 cases/4306 assertions pass with NoAssertions. The native
definition/type sweep now covers 27 neutral-transform keys. No production flow
algorithm, calibrated value, global default or project schema is modified.

macOS ARM64 Release application/native build exits 0. Selected CTest executes
197/197 without skips; six fresh OFF/ZAA comparisons pass under the established
timestamp and additive OFF-setting allowances. Evidence/B01-flow retains exact
commands, exit codes, red/final XML, discovery, baselines and source/binary hashes.
Author review traced the native factor applications and source pre-normalization
guard. Independent review, calibrated physical delivery, complete compatibility,
whole-job provenance and hybrid planning/export remain pending. Linux execution
of this revision and physical tests are NOT_RUN.
