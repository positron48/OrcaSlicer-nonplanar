# ADR-0102: prospective retained infill densification

Date: 2026-10-03. Status: bounded factory; independent safety review pending.
Parent: a7ee434104734050f3e274d3841da4e4517fc42e. Follows ADR-0101.

The clear-XY remaining-hatch factory cannot address missing target underneath
the prospective bead projection. Add a separate prospective replacement that
retains every original packet and dose and adds parallel fixed-width owners
between existing centres. All new roofs, finite footprints and doses use the
original actual body, before any cap deposition. This is a prospective material
construction, not a repair of already printed material or an approved route.

The explicit policy specifies maximum XY pitch, minimum covered gain, maximum
complete outside-target volume and maximum nominal repeated-material increase.
There is no default density or overlap allowance. The exact gap is subdivided
before allocation; actual stored binary64 centres must still respect the pitch.
Whole longitudinal extents and original target/contour policy remain. Original
contour/hatch owners precede new owners. End/width/corner combinations and a
second densification are unsupported until their complete recipes are qualified.

Private first-cap contract6 adds OriginalOwners/DensifiedOwners. New factory
contract1 owns before/after, policy and measured covered/missing/commanded/
repeated changes. Existing constructors keep OriginalOwners and unchanged paths.
Legacy end/width/corner replans refuse DensifiedOwners; the native job binder
also refuses it until a captured density program, private ownership and recipe
are added to the controller. Existing request/native-plan/manifest schemas stay
unchanged. The private version is not a printer-profile or 3MF migration.

Original packet error and proof limits are checked before retaining owners.
Retained plus new paths, packets, roofs, work, material records, cells, global
volume error and nested deadlines share one selected factory budget. Elapsed
milliseconds round upward for new child calls. Copy inputs before callbacks;
cancel, stale source, changed interval environment or exceptions publish nothing.
Complete joint nominal fill measures the original target and all material.
Insufficient gain, excess overlap/spill or unsupported geometry refuses.

Separate analytical fixtures rotate the complete body/ROI together for both
axes. Positive examples use explicit .4 mm original pitch, .18 mm selected pitch,
.45 mm fixed width and [.001,.001,2] mm3 gain/spill/repetition policy. Their
flat and .0004 mm-rise cases use the same original work and precision ceilings;
the .04 mm-rise coarse fixture retains its work-limit refusal. Earlier exploratory
cases and failed builds are preserved but excluded from final qualification.
The actual already dense native fixture retains insufficient-gain refusal.

Independent rational checks preserve old doses/sections, verify stored pitch and
the constant-flux stadium width law, and integrate four flat unions with exact
rectangle sweeps and rational square-root/pi bounds. Sloped unions have accounting
checks and existing roof tests, not independent arbitrary sloped certification.
No full filled cap, closed seam/contact/head/connector/order, captured density
program, byte geometry or export follows. Fixed17 stays four PASS/RUN and thirteen
UNKNOWN/NOT_RUN; full B01-B15 remains open and guarded export BLOCK.
