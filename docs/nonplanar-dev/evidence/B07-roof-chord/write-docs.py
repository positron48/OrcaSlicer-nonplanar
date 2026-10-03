from pathlib import Path
root=Path.cwd();d=root/'docs/nonplanar-dev'
(d/'adr-0104-first-finite-roof-chord.md').write_text('''# ADR-0104: certified first finite roof chord

Date: 2026-10-03. Status: bounded implementation; independent safety review pending.
Parent: 273c66386dc9c2842985e00039fbb5ca77c97df1. Follows ADR-0103.

On an actual rounded body roof, a constant whole-packet height approximation
exhausted the original first finite hatch segmentation budget. The transverse
native ROI already has an adequate immutable parent/strip integral, but its
finite path required a tighter continuous gap/dose approximation.

For first FiniteWidth paths only, try an affine roof chord when the original
constant bound cannot meet its gap or allocated volume error. Keep the constant
bound if the chord is unavailable or less accurate. Do not apply the chord to
Centerline diagnostics, raised actual-run floors or later gap-policy paths.
The nozzle path, selected surface, outer width, actual body journal, policies,
original resources and numerical margins are unchanged. No sampling-based
acceptance or implicit planarization is introduced.

A constant Z/h stadium is concave and C1 across the core. On a whole transverse
footprint with positive shoulder radicand, its curvature is bounded by
K = r^2/(r^2-u^2)^(3/2). The chord bow is at most K*(delta normal)^2/8 for every
width offset. A rectangle or entirely flat core has zero bow. One same row
covering the entire footprint supplies the lower chord. Each possible owner
supplies an independent upper chord plus bow; different lower endpoint owners
must never be joined across a valley. Changing Z/h or partial shoulders use the
original clipped whole-cell ceiling and cannot become a chord owner.

Outward intervals charge endpoint/height conversion, chord bow, actual-gap width
law and target/dose uncertainty. Every owner query/comparison charges original
work. Existing packet/roof subdivision, global volume error, deadlines, revision,
rounding environment and cancellation still decide success. Unsupported or
insufficient-resource cases return no snapshot. The fixed-width packet law and
D_nominal/D_upper/D_lower remain distinct.

The initial general-domain attempt changed accepted Centerline packet shapes
and caused the existing native fill positive to exhaust its original65535 cell
limit. Its failed full run is preserved. Restricting the improvement to the
requested finite-footprint path preserves that legacy diagnostic and its fill
proofs; neither its test nor its limits changed. A fresh full run qualifies the
corrected source, not the exploratory binary.

Four analytical single/three-row cases cover both axes. Five actual native
transverse paths positively fit the original single-path4096 roof/packet and
200000 work ceilings, sharing the cap's .0001 mm3 error allocation across
individual calls. They do not establish a shared deadline or complete cap.
The full transverse cap now constructs paths but still refuses its original
material union work limit; the parallel finite-width ridge still refuses depth.

An independent checker derives stadium widths from the complete actual journal
and dose. Exact analytic stationary/end extrema verify every continuous gap
without calling the planner's curvature estimate. Complete rational closed slabs
independently enclose ideal actual-gap volumes inside each captured target.
The valley mutation checks interior rejection even when its endpoints fit.
Its constant axis/butt scope is explicit; source geometry, Lower support,
complete fill, contact, head, route/order and physical qualification are open.

No public/profile/3MF or private recipe schema changes. Software hashes
invalidate dependent reports. Fixed17 remains4 PASS/RUN +13 UNKNOWN/NOT_RUN,
full B01-B15 IN_PROGRESS, export BLOCK. Next resolve the complete actual cap
union within original work/error budgets, then positive native density and
captured program/controller/final-byte ownership. Full layers/seams/contact/
head/routes/order/whole-job/source/software/physical/publication remain.
''')
(d/'B07-roof-chord-review.md').write_text('''# B07 first finite roof chord: critical author review

Date: 2026-10-03. ADR-0104. Author audit; independent safety review PENDING.

The chord improves only a previously inadequate constant FiniteWidth first
roof approximation. No nozzle/surface/policy, original work/error/time ceiling,
width/query footprint, contact exception or export permission changes. Existing
accepted constant packets are retained. Centerline, raised actual-run floors
and later gap-policy calls retain the old algorithm.

For constant Z/h body rows, outward area/width/projection intervals bound the
entire finite footprint. Stadium curvature uses the worst transverse shoulder
radicand, including every width offset. A single whole owner bounds the lower
roof by concavity; every other possible row separately bounds the upper roof.
Endpoint-owner switching cannot invent a chord floor through an overlap valley.
Partial shoulders/changing geometry retain clipped constant ceilings. Missing
owners or unsupported enclosures fall back to subdivision, not acceptance.

Endpoint roof-to-gap conversion is charged before packet construction. Existing
packet interpolation rounding, positive gap/section domains, actual-gap width
and integrated target/dose errors are charged again by their original consumers.
No consumer learns a tighter proof by retargeting an immutable certificate.
Work charges remain in the common evaluator; shared original polls/deadlines,
revision and interval environment remain. New negative fixtures exercise work,
roof/packet exhaustion, irreducible precision, stale/cancel/late cancel, callback
exceptions, deadline and hostile rounding. True cross-width ridge uncertainty
still refuses; a centerline success cannot qualify the finite path.

Independent Python proof decodes every canonical actual journal row and its
fingerprint, derives dose/width with rational pi/root bounds, and checks complete
finite butts. Continuous extrema come from exact endpoint/stationary algebra,
not the production curvature function. A same covering row supplies the lower
concave floor. All possible rows supply the upper. Every complete closed-slab
integral lies inside the captured target, not merely overlapping it. Eleven
mutations reject, including a flat bridge through a valley whose endpoints fit.
The checker supports constant axis rows/perpendicular paths only; it does not
independently establish source-to-plan geometry or physical/Lower qualification.

The initial Centerline regression is preserved as a failed full506 run. Its
existing native fill positive exceeded the unchanged65535-cell limit. The
FiniteWidth scope correction restores that positive; the final fresh run and
parent provenance establish the retained old accepted bytes/journals. Two
wrong-executable native focus attempts correctly executed zero cases/exit2 and
are explicitly excluded; the final native focus uses fff_print_tests.

Five actual native prospective paths use separate calls with divided .0001 mm3
error. Their observed aggregate counts/errors fit original single-path ceilings,
but neither their separate deadlines nor success proves a complete cap. The
original complete-cap material union still refuses200000 work; parallel finite
width still refuses depth. No positive native density, captured density program,
complete filled layers/seams/contact/head/routes/order/whole-job is claimed.

All private/public recipe versions, profiles/3MF/goldens and normative documents
remain. Software inventory binds changed code. Fixed17 stays4 PASS/RUN and13
UNKNOWN/NOT_RUN, overall UNKNOWN, export BLOCK. Platform/GUI/physical proof and
independent safety review remain distinct from local analytical evidence.
Standard U1 head/.4 nozzle are known; no printer or user preset action occurs.
''')
