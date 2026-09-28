# A06 author critical review

This is an author review, not independent A08 approval.

- No shared writer or GCode algorithm was modified. The adapter is callable by
  native tests only; it cannot open files or publish a job.
- Writer position is initialized from the explicit trusted starting position.
  A fresh instance excludes previous hops, E state and plate offsets. Test
  coordinates include a nonzero (100,200) placement. Production placement
  conversion itself remains out of scope.
- Every event validates identity, contiguous sequence and exact start continuity.
  Rounded-away motions, unsupported retracts and out-of-domain feed/E fail.
- Independent parser carries omitted Z and F and checks syntax without including
  Orca headers. Test-side independent arithmetic detects XYZ/E/F mutations.
  It does not validate support, collision, machine limits or deposited geometry.
- No safe/Verified/ALLOW flag is returned by either component. No standalone
  verifier executable, hash-bound manifest or final export gate exists yet.
- Numerical tolerances apply only to text round-trip. They are not clearance
  margins or evidence for a measured machine. Retractions, comments, startup,
  temperature/fan controls and arbitrary imported G-code are rejected here.
- Identity firmware mapping, initial position, mm units and fixed acceleration
  are external experiment prerequisites. Runtime confirmation and enforcement
  are NOT_IMPLEMENTED; this API cannot be promoted to guarded export as-is.
