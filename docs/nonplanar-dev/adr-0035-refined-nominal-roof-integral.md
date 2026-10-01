# ADR-0035: bounded integral above the highest deposited nominal roof

Status: implemented affine-cell query; full B06 and independent review pending.

ADR-0034 proves a complete first-pass gap interval and continuous D_lower support
from one owned actual material prefix. Refine its deliberately broad nominal
volume interval before any later thickness/path allocation. Internal integral
contract starts at 1; existing IR, ledger, body and transition encodings stay
unchanged. No persisted field/cache/profile/export format or migration is added.

The nominal target is `integral_A (affine_cap_height - highest_nominal_roof) dA`.
It is not the sum of bead volumes or union measure below the roof. Integrate each
convex exact XY proof cell once. Nominal rectangles have the motion's interpolated
top. For rounded sections, the roof is
`top-h/2 + sqrt((h/2)^2 - max(abs(n)-(w-h)/2,0)^2)`, where `w=A_flux/h+(1-pi/4)h`.
Outward longitudinal/transverse/height intervals bound this nonlinear expression
over every cell point. No sampled point accepts a volume result.

Clip each cell exactly to the existing nominal oriented bead enclosure before
computing a possible upper roof. A bead excluded from a parent remains excluded
from its exact-subset children. A roof lower bound is usable only when that bead
covers the complete XY cell (whole parameter range and permitted width). The
already-proven covered plane remains a floor when several beads jointly cover a
cell. Take the maximum of the candidate roof bounds, preserving overlaps and
steps. Retain all present previous/partial-current beads; future material is absent.

Polygon area and first moments give the affine cap integral exactly in eager
GMP rationals before conversion to outward binary64 intervals. Split closed cells
with exact half-planes, so children preserve the complete parent domain. Select
high-uncertainty cells first, generally transverse to a relevant bead or along a
height/end constraint. Split selection is a work heuristic, never a proof.

Maintain lower and upper sums separately in exact arithmetic when replacing a
parent by children. Subtracting interval parents would retain their uncertainty;
ordinary running double sums could lose the requested budget. The final outward
sum must have width <= the explicit global mm3 tolerance and a positive lower
bound. This is a total interval-width contract, not a per-cell stopping error.
Unrefined depth-terminal cells remain in the sum while other cells can refine;
exhaustion remains Unknown if the complete width cannot be met.

The combined preflight/roof/coverage/integral work and cells share declared limits.
Cancellation, source revision, rounding and the original whole-call deadline are
rechecked before publication. Native 36 mm2 evidence uses an explicit 8191-cell
budget for 0.01 mm3 width. The inherited 4095-cell default remains unchanged and
its refusal is retained; no geometry/width/margin or requested precision is relaxed.
Hard worker memory/deadline containment and full-cycle performance remain open.

Bounded is an arithmetic result with first-pass feasibility, not a selected bead
amount or E. Allocation to full paths, thickness choice, perimeter/edge/seam
corrections, contact/full-head CCD, legal order and final-byte independent replay
remain required. The native integration exercises actual Orca body paths but the
complete guarded planner still does not emit them or allow export.

Allowed scope: existing DepositionModel files, geometric/native tests, execution
records/status. Share the existing exact oriented roof projection with ADR-0034;
its old gap/roof checks and hash vectors must still pass. Original fixtures,
goldens, bundle, stock slicing/writer, printer profiles and user config stay intact.
Author audit is not independent safety review; physical qualification is separate.
