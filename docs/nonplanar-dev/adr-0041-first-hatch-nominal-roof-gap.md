# ADR-0041: first hatch amounts from the continuous laid nominal roof

Status: bounded first-centerline reconstruction implemented; full B07 and review pending.

ADR-0040 accepts declared affine gaps. The first cap lies above the actual
rounded native body roof, which need not coincide with the D_lower support
plane or an affine surface. Reconstruct the highest nominal laid roof along
one owned first-pass hatch centerline before selecting its constant-flux amounts.
Allowed changes: DepositionModel, geometric/native tests and execution records.

Add internal first-hatch-bead contract 1, with an immutable factory-only result
retaining the complete owned hatch/stack/material prefix. Capture input pointer,
line index and all policies before callbacks. Reuse the protected first-pass
integral's whole-ROI D_lower coverage and D_upper gap feasibility; the lower
support plane only supplies a valid lower bound on nominal roof height. Include
completed beads and the current fraction, excluding all future material. Reuse
exact oriented footprint clipping and continuous nominal section bounds on the
entire line, rather than endpoint samples or a nominal-layer label.

Bisect exact dyadic line parameters. At each node bound the highest nominal
roof: possible clipped bead heights raise the upper bound, and a bead covering
the whole segment can raise the lower bound. Candidate filtering is inherited
by children. A constant midpoint roof produces an affine gap approximant; bound
its bottom-height difference from the continuous roof, preserving the Z/gap
correlation. Reuse adaptive fixed-width constant-flux packets on that approximant.
Only the owned hatch's axis-aligned centerlines are supported: their stored
packet XY points must remain on the queried segment. Shared rounded endpoints
preserve within-line position continuity. Packet Z/gap rounding is charged to
the actual gap error; affine-model gaps may differ at a shared node boundary.
No pressure continuity or extrusion dynamics is implied by XYZ continuity.

The rounded ideal area is h*(w-c*h), c=1-pi/4. For gap error e, its deviation
from the approximant is bounded by e*(w+2*c*h_max). Multiply by stored XY length
and enclose the analytic affine target. This yields a whole-line interval for
the ideal amount over the actual nominal centerline roof, not a filled cap-cell
volume. Check every packet's actual constant area against the expanded actual
gap range, monotone width law and rounded-section domain. Allocate volume error
by exact original parameter fraction, reserving one quarter for the inner
packet integrator, and recheck exact sums against the final target interval.
No extra 3D slope multiplier is used.

Gap and width departures are explicit. Section fields remain affine-model
approximants, not an assertion that the real floor is affine. Both actual and
model widths lie within w +/- the reported width departure, so their edge
separation is at most that full departure. Sum parent numeric error, new endpoint
rounding, maximum gap error and full width departure for a conservative reported
numeric budget, capped at 0.05 mm. The native consumer charges this budget to
its declared material model; this does not qualify finite-width contact or
physical cap geometry over a transversely varying floor.

Defaults: gap approximation 0.001 mm, 4096 roof leaves, depth 24, query work 200000,
one second; inner packet defaults remain ADR-0040. Hard limits: gap 0.05 mm,
65535 leaves/packets, depth 32, work 200000 and cooperative deadline 30 seconds.
Outer and inner cancellation/current/deadline policies apply to the whole call.
No stale, cancelled, exhausted, unsupported or numerically uncertain call can
publish a partial snapshot. A stepped/discontinuous roof may remain UNKNOWN.

Existing material/IR/project/profile encodings are unchanged. There is no
persisted first-hatch cache to migrate. Future job/cache keys must bind this
contract version, the exact prefix, hatch policy/index, all approximation/work
policies, source revision and coordinate/error budgets.

This primitive reconstructs a first-line centerline gap and amounts. Transverse
floor/finite-width contact, overlaps and the actual filled union, perimeter/seam
allocation, support after cap deposition, travel/order/full-head CCD, motion/E
limits and final-byte replay/export remain required. The tight native fixture
produces many short packets; no firmware segment-frequency approval is inferred.
The public guarded-hybrid slicing/export blocker remains in force.
