# ADR-0045: First-hatch amounts over a finite nominal roof domain

Status: implemented bounded primitive; independent safety review pending.

## Invariant

The affine section-gap approximation of an accepted finite-width first hatch
encloses the continuous highest nominal laid roof over its complete butt-ended
XY strip. A roof query along the centerline cannot establish this invariant.

## Decision

Add `plan_first_hatch_footprint_bead` using the existing adaptive first-hatch
planner, fixed-width packet solver and shared continuous nominal roof bounds.
For nominal width `w` and permitted width departure `e`, query an exact strip of
half-width `(w+e)/2` before deriving amounts. Only accept packets when both their
declared and actual-gap width bounds satisfy that same departure. The whole
strip must stay inside the owned first-pass ROI, whose support plane was proved
continuously covered by D_lower. Nominal roof bounds do not substitute D_upper
for support or qualify tool collision/contact.

Finite butt planes, completed records and the actual current fraction remain
unchanged. Each adaptive child inherits only candidate beads intersecting its
parent query. The gap, width, whole-line amount and combined numerical bounds
retain the existing limits; failure or exhaustion publishes no snapshot. A
transverse feature that cannot be resolved by splitting along the path is refused.

Internal first-hatch contract 2 adds protected `FirstHatchRoofDomain` provenance.
The existing `plan_first_hatch_bead` retains its centerline diagnostic behavior;
its snapshot explicitly says `Centerline`. The new factory says `FiniteWidth`.
Both factories call one private implementation. No current persistence/cache,
project, material ledger, motion IR or profile encoding contains these snapshots,
so no external migration is needed. Existing consumers remain diagnostic; a
future acceptance consumer must require the finite domain explicitly.

## Evidence and boundary

Positive tests construct amounts on flat X/Y paths and across continuous rounded
shoulders. A higher off-axis ridge is rejected despite a valid centerline; its
unlaid remainder is absent. A real native body supplies a full flat bead core
for a bounded first hatch. Its test-only wider body paths are owned native settings,
not a modification or qualification of the user's printer configuration.

This bounds the axial gap approximant across the finite width. It does not prove
contact of the rounded side floor, filling of the target, local clearance to
D_upper or the head, seam, travel/order, later actual support or export. The public
guard stays shut and full B01-B15 remains active. Test counts, exact commands,
raw failure history and hashes are in `B07-first-footprint.md` and its evidence.
