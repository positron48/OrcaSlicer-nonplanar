# B03 whole-segment nominal footprint containment

The entire XY capsule must fit strictly inside one audited connected mask. The
analyzer now exclusively constructs immutable projection snapshots. The query
captures its snapshot, radius, boundary uncertainty, transition inset and limits
before callbacks. It checks exact start membership and exact rational squared
distance between the whole candidate segment and every outer/hole boundary edge.
The three inset components are accumulated outward; equality rejects. No scaled
integer offset, coordinate snapping, rounded distance or point sampling is used.

Three predefined cases first failed against an UNKNOWN-only query (exit 42).
The implemented positives fit an explicit block/wedge ROI and pass beside a
hole; the negative crosses that hole despite individually admissible endpoints.
Exact tangency rejects while its immediately smaller binary64 radius passes.
An independent rectangle half-plane oracle agrees for 120 deterministic
capsules. Additional tests reject invalid inputs, late staleness, timeout,
exceptions and changed rounding, and retain captured ownership even if a
callback destroys the caller's snapshot and changes query/limits.

macOS ARM64 Release app/native build exits 0. The combined projection/footprint
selection passes 15 cases/340 assertions with NoAssertions. Selected CTest runs
173/173 cases with no skips. Six fresh OFF/ZAA runs and byte/replay comparisons
pass with only the established timestamp normalization and additive OFF setting.
evidence/B03-footprint contains commands/exit codes, red and final native XML,
CTest discovery/results, baseline manifests and source/binary hashes.

Author review checked the connected-capsule argument, hole membership, exact
distance kernel and final cancellation boundary. Independent review remains
pending. This is an implicit nominal XY inset query, not an area-clipped ROI,
curvature/Z-contact, physical bead model, head/scene access, body/cap reservation,
job/export qualification or Linux/physical result. Those tasks remain open.
