# ADR-0034: affine first-pass feasibility from actual deposited material

Status: bounded implementation; full B06 and independent review pending.

Use one owned material prefix as the source of all three representations. Copy
its handle, the physical affine cell, gap policy, requested support plane and
limits before callbacks. Retain these inputs in the result. No caller-supplied
CAD roof or single inflated support surface enters the decision.

The support plane is a candidate, not evidence. Prove its complete XY footprint
continuously inside D_lower using contract 1 coverage. A missing footprint
rejects this candidate; an uncertain or exhausted proof remains Unknown. A
footprint may span several actual deposited rows and a stepped nominal roof.
There must be a common covered plane for this bounded query; no such plane is
invented, and a wider varying-height cell may need subdivision by the future solver.

For each present bead, derive an oriented XY enclosure and a roof ceiling from
its existing constant-flux rectangle/stadium section. Outer XY/Z growth,
coordinate uncertainty and varying-gap/height derivative terms are the same
assumptions used by material membership. Clip the requested convex footprint
exactly against longitudinal/transverse enclosure half-planes before bounding
height. This preserves XY correlation: a neighboring tall diagonal wall with an
overlapping axis-aligned box need not become the roof under a low cap. Any bead
that might overlap is retained conservatively. Previous and partial-current
geometry remain; future beads never enter the scan. Ceilings may overestimate
rounded shoulders and therefore block a feasible candidate, never approve one
through sampling or exclusion of uncertain material.

A covered lower plane bounds the highest guaranteed support from below; the
upper ceiling bounds the highest possible material from above. Bound the entire
affine cell height, including the fourth derived corner and independent errors
on the three inputs. Compatible requires the whole resulting gap interval
strictly inside the declared limits and a positive nominal-volume lower bound.
A corner certainly too low relative to guaranteed material, or certainly too
high relative to the upper ceiling, rejects. Other uncertain gaps remain Unknown.
No surface or Z is moved as a response.

The nominal roof lies between the covered plane and its nominal ceiling. Bound
`integral_XY (affine_height - highest_nominal_roof) dxdy` with outward arithmetic
and the exact affine mean `(z10+z01)/2`. This is a signed diagnostic interval for
unfilled vertical cell volume, not the cumulative commanded material amount,
the measure of overlapping bead union, a selected bead amount, or V-to-E proof.
Refinement, edge corrections, volumetric seam and a qualified path-volume solve
are still required. Choosing the interval midpoint is not authorized.

One total evaluation budget covers both roof scan and support proof; their
combined deadline, cancellation, revision and rounding state are rechecked
before publication. Cooperative bounds do not provide hard worker/RSS isolation.
Internal material-transition contract starts at 1. This adds no persisted field,
cache, profile setting, MotionEvent/ledger encoding or public export format;
existing hash vectors and consumers remain unchanged. A future job/cache owner
must bind the retained cell/policy/plane and source identity, not only a status.

Allowed scope: existing DepositionModel files, geometric/native tests, execution
records and development status. No stock slicing/writer path, original bundle,
golden, printer profile or user config changes. The implementation is exercised
by native integration tests on actual reserved-body paths; the complete guarded
planner still does not call it or allow export. Author review is not independent
safety review. Printer qualification remains separate.
