# B03 isolated nominal upper analysis

The native worker now runs the upper analyzer on its owned STL geometry when
explicitly requested. Protocol 2 binds the analysis kind, revision and source
SHA-256. The parent validates the exact bounded schema and summary counts,
area/height/slope ranges and patch/crease consistency before publishing a result.
Geometry validity remains separate from nominal upper-analysis success.
The default geometry-only CLI is retained; the new invocation is
`nonplanar_stl_audit --millimeters --upper INPUT.stl`.

ADR-0024 defines the fixed 0.2 slope and 2500 upward-facet domain. The CLI
identifies source-model coordinates and nominal-only scope. Export is always
false, including a successful analysis with no slope-selected area. Source files
are unchanged. Neither plate placement nor tool accessibility is inferred.

The initial positive native test fails with absent upper data (exit 42). The
implemented worker passes seven combined worker cases/134 assertions, including
three new cases. They cover owned source/options, ten malformed protocol probes,
UNKNOWN projection, cancellation, stale revision, callback rounding changes and
observed memory limits. Existing process failure/deadline/source-identity tests
remain. Author review checked final publication, argument validation, count
arithmetic and numeric environment. Independent critical review is pending.

All 19 actual CLI cases pass: the original ten geometry cases plus flat/shallow
and steep upper surfaces, malformed input and usage errors, and a separate
28-face C extrusion. Its geometry encloses 1100 mm3, but its hidden shelf gives
UNKNOWN overlapping upward projections. The first CLI run used an incorrect
600 mm2 expectation for the 32 x 20 wedge; the catalog establishes 640 mm2.
Only this new expectation was corrected, and the failure log is retained.

macOS ARM64 Release application/native build exits 0. Selected CTest executes
195/195 without skips. Six fresh OFF/ZAA comparisons pass with only established
timestamp normalization and additive nptop_mode=off allowances. Evidence includes
commands, exit codes, red/green XML, CLI bytes, test discovery, baseline manifests
and exact source/binary hashes under evidence/B03-worker. No build regeneration
was needed; the embedded upstream build macro is not used as the source identity.

The older Linux run 36434944242 at 01865dcf is still building application/tests
in the retained 16:08 UTC observation; these changes have no Linux test proof.
Curved-surface bounds, ROI/tool access, real plate/job ownership, planning,
guarded export, hard RSS containment and physical qualification remain open.
