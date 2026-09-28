# B02: recheck interval arithmetic after callbacks

Invariant: a callback cannot switch the computation to an unsupported rounding
environment after the initial guard and still receive accepted geometry/bounds.
The geometry audit and source-conversion proof previously checked the environment
only before calling application cancellation callbacks. A callback setting
FE_UPWARD or FE_DOWNWARD then returning false bypassed that declared prerequisite.

Two native regression cases reproduced eight failed expectations (exit 42):
geometry returned ValidGeometry with an accepted mesh, and both binary/ASCII
source-bound calls returned instead of rejecting the changed environment. This
proves a guard bypass; it does not by itself prove that those particular returned
numeric bounds underestimated their true error. The unsupported state must fail
closed regardless. The red XML and commands are retained, not replaced.

The fix rechecks the established interval environment after every cancellation
callback before protected calculations continue. Existing cancellation behavior
and the callback's own rounding state are preserved; the library does not silently
reset the caller's state. The new placement boundary already made this check.
Collision/transition/profile modules have no application callbacks at this boundary.
This change is confined to the guarded geometry/import helpers; stock parsing,
placement, slicing and writer paths are unchanged.

macOS ARM64 Release: incremental native builds exit 0. Both cases pass all 17
assertions after the fix. Selected CTest executes 127/127 with no skips/failures;
the ten actual diagnostic CLI cases still produce their expected outcomes.
The normative package audit passes. Commands/exits, red/green XML, full CTest
selection/results, CLI reports and source/binary hashes are in evidence/B02-callback.
No fresh full app/OFF/ZAA run was performed for this internal helper fix; the six
B02-snapshot comparisons remain the latest. Linux run 36413130735 has completed
dependency compilation and is building its earlier native application/test commit.
No remote, independent-review or physical pass is inferred.
