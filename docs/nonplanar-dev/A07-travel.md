# A07 owned whole-head simulation travel

The new native orchestrator captures scene/events/policy/limits before callbacks,
checks declared coverage and inventory, and checks every Travel event against
every head component and static box. It includes the finite annular tip using
A04's new box pair, preserves event/head IDs, records the generated tip ID and
one-based obstacle index, and returns the limiting or first blocking check.
Partial diagnostics never grant PASS. Pair-count limits use overflow-safe
division; per-query evaluation limits share the overall deadline. Cancellation,
staleness and arithmetic-environment checks cover final publication.

Geometry contract version 4 adds the explicit scene_geometry error term with a
zero default. Scene uncertainty is charged for both interacting envelopes.
Coverage additionally includes every caller budget term and required clearance.
Author review found that initially only pair distances charged those terms,
allowing an empty scene to pass despite envelopes leaving the known domain.
The independent negative-X bound produced 10 red assertions; coverage inflation
now rejects all five uncertainty terms and excessive required clearance.

The original low floor started at Z=0.2, exactly on the expanded 0.1+0.1 coverage
boundary. Outward arithmetic correctly could not prove that boundary after the
coverage fix. The positive fixture's lower floor face is now Z=0.25, leaving an
analytic 0.05 margin; its top/clearance and all expected outcomes are unchanged.
Failed XML is retained. No runtime tolerance or required margin was lowered.

Predefined positives check all 14 pairs for one travel and 28 for two. Negatives
find a duct-only collision between clear endpoints, block an uncertain tip gap,
unsupported material states, incomplete/operator-claimed scenes, exhausted work,
late revision/cancel, timeout and changed rounding. Mutating/destroying the
caller's scene/events/policy/limits in a callback does not alter captured work.
An explicitly complete empty synthetic inventory has zero pairs; its coverage
and policy must still pass. It is not a real-printer clearance certificate.

macOS ARM64 Release app/native build exits 0. Combined A07/geometry tests pass
33 cases/1006 assertions with NoAssertions; selected CTest executes 186/186
without skips. Six fresh OFF/ZAA comparisons pass with the established timestamp
normalization/additive OFF setting. Evidence records exact commands/exit codes,
red/final XML, discovery, baselines and source/binary hashes. A final test-only
rebuild removes trailing whitespace and repeats the native/CTest checks.

Independent review, real job ownership, full material chronology/contact,
arbitrary mesh scenes, physical qualification and export remain pending. PASS
from this API applies only to the captured synthetic static-box Travel domain.
