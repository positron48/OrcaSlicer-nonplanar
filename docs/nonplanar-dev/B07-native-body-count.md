# B07 native body preservation uses the actual source count

Linux parent run 36957287331 at 458537e8 passes the original five-second
end/width replan and extended-void calculations that failed in earlier runs.
It then fails the later material test at `body_records == 2034`: the same pinned
native fixture has 2073 source records on that runner. The literal is a macOS
observation, not a normative geometry or cross-platform record-count contract.

Require the appended body's count to equal both the owned pre-append assembly
count and the actual native body record vector size. Existing exact canonical
comparisons of every body row during assembly and every old ledger row during
later append remain unchanged. Thus no source record can be lost or fabricated;
the test no longer substitutes a platform-specific count for the real input.
No geometry, amount, interval, tolerance, expected mask, profile, production code,
resource limit or failing geometric case is changed.

Focused complete native first-cap/later-material/motion case runs on macOS ARM64
with Apple Clang 21; its exact command, exit, JUnit and source/binary hashes are in
evidence/B07-native-body-count/source-manifest.json. The previous full macOS gate
at 4903be4a passed 360/360 and six OFF/ZAA pairs. This test-only correction does
not rerun those unchanged production paths. Linux verification of the corrected
source is pending; the original failure annotations are retained.

This is an author source review, not independent qualification. The assertion
reads two original authoritative inputs, and existing row-by-row canonical checks
retain geometry and semantics; no golden or original input changes. All native
paths after the old early failure still require observation on Linux. Full B01-B15
stays IN_PROGRESS; physical work is NOT_RUN and guarded export remains BLOCK.
