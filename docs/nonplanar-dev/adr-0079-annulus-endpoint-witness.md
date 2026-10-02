# ADR-0079: refute short-bead annulus travel endpoints

Status: bounded B08/B09 diagnostic improvement; full tasks and independent review
remain pending. Scope: existing DepositionModel and its two native test consumers.
Original normative contracts, geometry, margins and budgets remain unchanged.

The full native wedge exit was UNKNOWN on its first leg because interval
subdivision reached a required-clearance boundary before finding an interior
Upper intersection. Temporary instrumentation identifies component0 / row2210,
time[0,.00048828125], local XY[-.138671875,-.1884765625]..[-.13818359375,-.1875].
Independent 70-digit arithmetic finds a different strictly interior annulus point
at original time0: local[-.11399619238705583,-.16468801247189135,0]. Its squared
radius is .0401172733306884873, greater than the original opening squared .04
and less than outer squared .25. The rounded Upper squared-radius surplus is
.0001428231577216089. This refutes the declared envelope without required-margin
padding; it does not assert a measured physical or nominal collision.

Keep the complete continuous acceptance partition unchanged. Only improve
FAIL-only probe selection: at an unresolved root cell try the original two
endpoints and midpoint. For an annulus also project the material's start,
midpoint and end onto its XY tangent, then construct both points on a circle
near the opening. Its radius is opening+(outer-opening)/1024. This is only a
heuristic point choice, with no accuracy/contact/clearance significance.

Every candidate remains in the original local tool cell and must pass strict
interval annulus membership and strict Upper membership at the original time.
Current material uses that same time as progress; future rows remain absent.
Required margin is still used for all continuous exclusion leaves. No point,
empty probe list or exhausted budget can produce PASS. Charge each projected
anchor and candidate to the original total work/deadline/cancellation/revision/
rounding checks. Child cells retain the original midpoint probes. No depth,
loss, margin, contact, component or numeric allowance is changed.

The original native exit now gives FAIL / MATERIAL_MOTION_UPPER_INTERSECTION,
component0 / first actual later packet / time0, with no route snapshot. Preserve
all original body/cap/later rows, pressure and byte candidate; no new height can
repair a forbidden original starting pose. Replan the complete cap/end/contact/
access relation before constructing a permitted departure. Guarded export stays
BLOCK. Runtime contracts, canonical journals, profile and 3MF formats do not
change; no persistent proof cache is introduced.
