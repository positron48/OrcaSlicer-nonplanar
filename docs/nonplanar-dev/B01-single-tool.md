# B01 single-tool domain and pre-normalization rejection

Invariant: unsupported material/tool selections cannot become eligible through
native normalization or an unchanged export cache. ADR-0016 defines the bounded
single-nozzle/single-filament rules and retained-input rejection.

Added 14 native type/cardinality/selection rules and six discrete restrictions.
Five new cases exercise valid inheritance/first-material selection, extra/empty
vectors, malformed/native-nullable values, NaN/infinity/subnormals, nonidentity
offsets, fixed maps, automatic switching, material painting, all four sparse
model source scopes and actual Print cache invalidation. Native source inputs
are preserved. The existing discrete tests now check all 16 entries, including
missing fields and a misleading generic enum dictionary for filament mapping.

The initial four-case run failed with 190 assertions and one test-input error:
fmmDefault exists as a native enum but has no text deserializer entry. The test
now supplies that native value. A later integration test independently exposed
an actual stale-cache case: setting source extruder=2 on a one-filament model
left psGCodeExport done. Capturing the diagnostic before native normalization
and invalidating its changes fixes that case. Tests additionally prove that the
resolved prime-tower flag is false while its requested true value still blocks,
and that clearing the conflict invalidates the cache and restores only the
bounded preflight. OFF retains the source settings and has no guarded rejection.

Final macOS ARM64 Release app and native targets build with exit 0. The header
change required a full affected native/GUI rebuild (299.374 seconds); the final
test-only rebuild also succeeds. Thirty-one B01 cases/3321 assertions pass with
NoAssertions enabled. Fresh selected CTest executes 158/158 without skips or
disabled tests. All six OFF/ZAA differential cases pass; only the existing
timestamp normalization and additive nptop_mode=off setting are allowed.
Commands, exit codes, failed/final XML, manifests and binary/source hashes are
in evidence/B01-single-tool. Initial compilation corrections and the no-op build
of the unrelated nonplanar_tests target are retained in command metadata;
test_policy.cpp belongs to fff_print_tests.

No material calibration, physical printer qualification, Linux test pass,
independent review, complete compatibility schema or hybrid output is claimed.
The stored diagnostic is not the immutable whole-job snapshot. All guarded
slicing/export remains blocked; surface/body/cap planning remains separate work.
