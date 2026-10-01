# ADR-0040: fixed nominal width with adaptive constant-flux packets

Status: declared affine-gap bead amount primitive implemented; complete B07/B10 and review pending.

ADR-0039 distinguishes finite volume cells from their boundary remainder. A cell
amount still cannot directly supply E for a rounded bead. ADR-0030's existing G1
model has constant area within a motion event: width is A/h or A/h+(1-pi/4)h.
An affine varying gap cannot simultaneously have exactly constant physical
width and one constant-flux event. Keep one fixed design width and subdivide the
path until actual model width departure and total volume error are bounded.
Allowed changes are DepositionModel, geometric/native tests and records.

New internal fixed-width-bead contract is 1 with a factory-only immutable
snapshot. Capture a declared straight physical path, affine endpoint gaps,
fixed nominal XY width, section kind, revision/source context and limits before
callbacks. The context string is not proof of actual support or source geometry.
Native tests derive prospective later gaps from the owned affine surfaces;
actual first-roof and later deposited support remain separate obligations.
Existing material/IR/profile/project formats and earlier internal contracts are
unchanged. No persisted primitive cache exists. Future keys include this version,
the complete request/source, numerical/approximation policies and revision.

For XY length L, fixed width w and affine h0/h1, the analytic target is
L*(w*(h0+h1)/2-c*(h0*h0+h0*h1+h1*h1)/3), where c is zero for rectangles and
1-pi/4 for rounded transverse sections. Butt ends match the current material
model. No extra 3D-length multiplier is applied. A packet's commanded amount is
its actual stored XY length times h_mid*(w-c*h_mid). Its area is then the actual
binary64 amount divided by that XY length, matching the existing capture/query
law. The midpoint nominal-width anchor must remain valid.

Store subdivisions in eager exact dyadic parameter intervals, with shared
rounded endpoints for position continuity inside a line. No pressure dynamics
are inferred. Bound endpoint
XYZ/gap rounding separately. Compute the full packet width range from its
actual amount, minimum/maximum gap and rounded-section domain; reuse the same
XY-length and section-width operations as MaterialRecord capture. This range
models approximation around the same design width, not a variable-width design.
The actual material ledger still carries and checks that entire range.

Compare each packet amount against the analytic target over its exact fraction
of the original path. Allocate the whole-path volume error in proportion to
that fraction, never as a repeated per-packet allowance. Bisect if width, amount,
domain or midpoint-anchor bounds cannot accept. Sum stored amounts exactly and
check the final whole-path error again. Rectangle affine-gap integration is
linear; rounded midpoint integration has a quadratic defect that subdivision
must control. Descending gaps use the same bounds as ascending gaps.

Defaults are 0.005 mm width departure, 0.001 mm3 total amount error, 4096 packets,
24 bisection levels and one second. Hard maxima are 0.05 mm width departure,
65535 packets, 32 levels and a 30-second cooperative deadline. Bound request
context before copying. Staleness, cancellation, changed rounding, work/depth,
numerical/domain/precision failures cannot publish a partial candidate. Numerical
endpoint error and width approximation remain distinct future budget inputs;
parent import/geometry errors must also be charged by the eventual job consumer.

This stage selects nominal per-line bead amounts, not a filled cap material
union. Adjacent overlaps, transverse voids, boundaries/perimeters/seams, actual
first-roof gaps, subsequent support, contact/full-head CCD, legal line order,
V-to-E/firmware/motion limits, final-byte replay and unified export remain
required. The native consumer test accepts each prospective later line in the
existing declared material model, without inventing an executable multi-line
order. The existing safe-hybrid slice/export blocker remains in force.
