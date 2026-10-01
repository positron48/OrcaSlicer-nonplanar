# ADR-0060: actual body and active first-cap material for local later support

Status: implemented bounded software primitive; independent review and complete
later-pass construction remain pending. Scope: DepositionModel and both native
consumers. No changes to the original normative bundle, presets or export gate.

## Invariant

A later query uses exactly the completed original body prefix and the selected
actual fraction of the original first-cap ledger. Future body records are omitted;
future cap records remain owned but do not contribute material. A prospective
surface is a target, never evidence of occupancy or support.

`reconstruct_first_cap_material` owns a protected assembly. Completed body records
retain every original field, event ID and sequence index. Cap rows retain XYZ,
flux, sections, metadata and labels, with unique global IDs/indices and an explicit
mapping back to the original ledger. An explicit connector travel connects the
last body event to the cap start; this declares a scenario without qualifying the
move. The original cap/body and target remain owned by the assembly.

The factory currently requires a body event boundary. A partial body event is
refused rather than flattened into rounded replacement endpoints and a changed
section. The selected cap prefix uses material_at, including its actual current
fraction. Runs contain only active contiguous axis-aligned cap packets; travel,
turns and future packets are not merged. Original model IDs and losses must agree;
the common numerical coordinate error is their maximum. Nominal geometry stays
exact while upper/lower use this conservative error. Shared record/work/deadline
budgets and final cancellation/revision checks publish no partial snapshot.

## Explicit support and local later surface

`cover_first_cap_material_lower` certifies a whole box through one actual
continuous cap-run lower proof or the original independent-event lower union
of the composed body/cap. The proof records which model supplied support.
Original per-event LowerMaterialView semantics remain unchanged. A nominal run
AABB can prune impossible proposals but cannot certify occupancy. Unsupported
combined partial-run coverage, numerical uncertainty and child budget failures
remain refusals; this primitive does not yet compose a plane across several runs.

`assess_first_cap_next_pass` crops a local query rectangle on an original later
prospective affine surface, preserving the original complete ROI and final target
in the source. Exact affine interpolation and storage error propagate the original
corner uncertainty into the owned effective policy. It queries nominal/upper roofs
from the active assembled prefix, requires an explicit whole lower support plane,
and applies the original later vertical-gap policy to the complete local footprint.

The existing first-pass roof/gap arithmetic is shared privately with this query;
its original independent-event support check remains unchanged. The diagnostic
volume bounds refer to the local stored affine cell above the actual nominal roof,
not a selected dose/E or a filled complete later pass. A compatible local query
does not authorize skipped passes, connector motion, normal thickness, contact, order or export.

## Runtime contracts and verification

Add first-cap-material, first-cap-support and first-cap-next-pass contracts version
1. Existing material representations, first-cap version 3, IR/3MF/profile/default
schemas and caches remain unchanged. Protected wrappers own the original source
and the composed material prefix, whose fingerprint binds model and ordered rows.
Rebuild the application and both native consumers.

Positive analytical fixtures cover both axes and flat/sloped surfaces. Independent
113-bit amount sums, affine interpolation and whole lower-box/run oracles verify
preserved fields, source ownership, active fractions, support and gap bounds.
The native wedge uses its original sliced body and mixed-width, end-extended cap.
Every native body record is checked for exact preservation. Local second-surface
support spans real internal packet cuts; future-cap and full-ROI requests refuse.

Full target deficit, allowable excess, shoulder/seam geometry, general run-union
support, partial-body continuation, actual later beads, head/contact/order/flow,
job/replay/export integration and physical qualification remain open. Detailed
commands, historical failures and hashes are in B07-first-cap-material.md.
