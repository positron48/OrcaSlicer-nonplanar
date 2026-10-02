# ADR-0068: complete journal linear motion limits

Status: complete supplied-journal simulation contract implemented; full B10,
firmware/export enforcement, independent review and physical qualification pending.
Scope: MotionPlan, its isolated CMake registration and both native consumers.
The original SPEC, DATA_CONTRACTS and backlog remain normative and unchanged.

## Invariant

Every original journal row receives an owned motion step. Preserve every pose,
deposited amount, bead/pressure metadata, event ID, annotation and order. Only
XYZ speed/acceleration ceilings may decrease. Retraction speed is a separate
filament-coordinate step, not a reinterpretation of the IR's XYZ speed fields.
An instant Travel receives an explicit minimum dwell obligation.

The admitted model is piecewise straight motion with a full stop at both ends
of every event. Unsupported firmware lookahead, measured/confirmation claims
and unknown kinematics refuse. Cartesian drives use X/Y/Z; CoreXY drives use
X+Y, X-Y and Z. Check both Cartesian axes and mapped drives. A future consumer
must enforce stops, acceleration and dwell; a string of ordinary blended G1
moves cannot claim this certificate. All transforms are identity in this
simulation domain. Actual firmware qualification remains a separate requirement.

For XYZ distance L, outward intervals bound every absolute axis/drive component
divided by L. Reduce original path speed and acceleration to satisfy all mapped
rates. For deposition retain original nominal V, derive commanded equivalent
volume V*k and filament E=V*k/(pi*d^2/4), with flow applied once. Check commanded
cross-section V*k/L and peak commanded-equivalent Q; reduce speed/acceleration
for E velocity/acceleration as well. No 3D/XY multiplier changes V. This declared
command calculation does not calibrate delivered physical material.

Pure-E retract/restore uses its original signed filament amount, max length,
filament speed/acceleration and event-rate policy; no phantom deposited volume.
Per-event speed is at most L*max_events_per_second and sqrt(a*L). Thus the
rest-to-rest trapezoid duration L/v+v/a is defined for the complete interval and
at least one event period; short segments become triangular or slower. Full
stops avoid attributing unmodelled junction/curvature acceleration to a straight
segment. Curved input must first satisfy separate chord/segmentation requirements.

## Ownership, numerics and versions

Capture protected scene/material source, policy and limits before callbacks.
One root allowance/deadline covers policy validation, every source row, each
axis/drive classification, rebuilt material and scene preparation, and final
publication. Original material/scene and separate motion-policy revision checks,
cancellation and rounding remain active. No failed or partial plan publishes.

All arithmetic uses outward binary64 intervals under strict compilation without
PCH. Bounds for pi are explicit. The existing filament_feed command must lie in
the independently constructed E interval. Divide a ceiling by a small component
only when its current rate exceeds that ceiling, avoiding irrelevant overflow
for zero directions while retaining their bounded checks.

linear_motion_plan_version is 1. The exact policy fingerprint includes every
identity, model/kinematics, domain, axis/drive/extrusion/rate and diameter/flow
field. Revalidate the complete changed raw ledger with existing factories;
changed speeds/accelerations update its existing ordinary fingerprint. Old
motion/job certificates must not qualify the new source. There is no persisted
proof cache or change to existing material/scene/IR/3MF/profile schemas.

## Remaining full scope

Native full body/cap/later records pass the limiter; this does not qualify their
geometry/contact/order or the B09 exit that remains UNKNOWN. Complete cap volumes,
flow calibration, actual firmware/transform/junction contracts, exporter stop/
acceleration/dwell enforcement and independent replay of final rounded bytes are
required. No production route or physical profile is approved. B01-B15 stays
IN_PROGRESS and guarded export stays BLOCK.
