# A02 follow-up: deterministic Print defaults

The completed Linux run 36434944242 at 01865dcf built the application and tests,
then failed `Skirt height is honored` with `Coordinate outside allowed range`.
Its public annotations are retained in `evidence/B03-heights/`. This failure
motivated an audit of the native helper's default-constructed `Print`.

Both `m_origin` and `m_isBBLPrinter` had no initializer, as in pinned upstream.
`Test::init_print` sets neither field. `Print::export_gcode` passes the origin
to the native writer and processor as the plate offset; travel geometry later
uses that offset before polygon operations. The printer flag also controls
native processing and export branches. Indeterminate state is a demonstrated
defect and a plausible contributor to the Linux exception, but the annotation
alone does not establish its exact call stack or root cause.

Initialize the native plate origin to `Vec3d::Zero()` and the printer flag to
`false`. Existing GUI/CLI setters retain their supplied values. This is a
constructor correction only: no profile/schema migration, policy relaxation,
test exclusion or slicing algorithm change. Allowed source scope is the two
member initializers in `Print.hpp` and the focused test in `test_print.cpp`.

The regression default-constructs `Print` in aligned storage filled with 0x01,
which supplies nonzero finite double representations and a valid true bool on
the supported targets. It checks all three zero coordinates and generic-printer
state, then verifies that an explicit nonzero origin and Bambu identity can
still be assigned. A custom deleter destroys the placement-constructed object
without freeing its stack storage, including if an assertion unwinds the test.
Before the correction, the four default checks fail (exit 42); both setter
checks pass. Catch's decimal display rounds the tiny nonzero coordinates to
`0.0`; the exact equality checks still fail. This reproduces the initialization
defect, not the Linux polygon exception.

The macOS ARM64 Release build of the application and both selected native test
executables passes with Apple Clang 21. The new case passes all six assertions
with `NoAssertions`; the two native skirt/brim cases pass nine assertions with
the failing Linux seed 1648739313. Selected CTest discovers and executes 208/208
cases without skips (37 original cases and 171 added cases). All six fresh
OFF/ZAA captures match stock G-code and modal replay under the existing timestamp
normalization and additive `nptop_mode: off` setting allowance. No golden changes.

The first combined run with `NoAssertions` exits 42 because the inherited skirt
test puts its common assertion after its two sections. Catch diagnoses both
sections as having no assertions, although the actual height assertions pass.
The console diagnostic and failed JUnit are retained; this is the previously
documented stricter-test limitation, not a polygon exception. The ordinary
native CTest policy is unchanged, and the new regression retains NoAssertions.

Exact commands, exit codes, red/green XML, full discovery/results, baseline
manifests/comparison, author review and source/binary hashes are in
`evidence/A02-print-defaults/`. Raw build/capture output remains under
`build/nonplanar-evidence/print-defaults*`. Source inventory audit passes; it is
not a semantic or safety approval. Existing native build warnings remain.

Linux verification was refreshed on 2026-09-30: native run 36454696019 at the
corrected d57e9325 revision completed successfully. Its Linux job reports success
for building native tests, executing the nonempty selected native suites,
exercising the STL audit CLI and building the application. Live GitHub API
run/job/step records are retained under evidence/B01-print-snapshot. The workflow
restored exact cached dependencies; it did not rebuild dependencies in this run.
Archived JUnit counts/compiler output were not downloaded, and no Linux OFF/ZAA
differential capture is inferred. The earlier failing run remains historical
evidence; success confirms the corrected revision without proving the exact
exception call stack.
Independent review, Windows execution and physical qualification remain pending.
This correction does not complete Gate A, B01–B04 or authorize hybrid export.

The implementation commit resolves with
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/A02-print-defaults.md`.
