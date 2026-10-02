# ADR-0062: sequential actual later-cap material with owned bead provenance

Status: bounded software path implemented; complete cap/job/motion qualification
and independent review remain pending. Scope: DepositionModel and both native
consumers. The existing first-cap-rooted material snapshot now also owns later
paths. No global type migration, normative/preset/schema or export change.

## Invariant

Appending later paths preserves every previous material row and origin exactly.
New packets retain their original absolute XYZ, section, width and dose, receive
unique global IDs/indices and map to their protected source bead/piece. Only the
selected complete appended records and the actual current fraction contribute
material. An unlaid future bead never supplies a roof or Lower support.

`append_next_cap_material` accepts an ordered batch calculated on this exact
complete prefix. Capture input owners/count/limits before callbacks. Reject a
partial or incomplete previous ledger; silently dropping its future rows or
rounding a current endpoint would change the declared sequence and geometry.
All batch owners must use one original pass, either the current last pass or its
immediate successor. This permits extending a partially covered pass spatially,
without claiming that it is complete or authorizing arbitrary pass skips.

## Source consistency and sequence

Each proposed batch dose used the same original actual roof. Its entire packet
Upper XY projections, including unchanged outer growth and the common numerical
error, must be disjoint from every other batch path. Otherwise the factory
refuses: append a path first and re-plan the next against the resulting actual
prefix. Passing an old dose/source again is rejected. No overlap exemption or
use of a future batch row as support is introduced.

Copy all old rows/IDs/indices/origins unchanged. Add explicit connector Travel
where necessary, followed by original packet rows in the caller's order. Material,
patch, speed/acceleration and contact/provenance context are inherited from the
actual prior deposition context; these remain declared values, not newly
qualified motion/contact/flow limits. The retained bead owner binds its actual
source/support/roof/amount proofs. No downstream sorting or extra Z/E conversion.

The common numerical coordinate error is the maximum of the previous model and
new bead errors; original inner/outer losses remain unchanged. Nominal old/new
geometry and doses stay exact. A larger error can tighten Lower or expand Upper
for old rows conservatively; a future owner never adds occupied nominal geometry.
Rebuild every active old/new continuous run against the newly owned prefix,
including the actual current front, and share record/path/work/time/callback
budgets through capture, prefix construction, run reconstruction and publication.

`FirstCapMaterialSnapshot` retains its original first-cap/body/final-target roots
and now owns a cumulative list of protected later beads. Later origins identify
the global owner and original piece. For a run, `path` indexes original cap paths
when `origin` is FirstCap, and cumulative later owners when it is LaterCap.
Material runtime contract becomes 2; support and next-pass become 3; next-bead
becomes 2 because its source can contain actual later material. Existing
material-ledger serialization/fingerprints already bind all model/ordered rows;
no persisted schema/cache/IR/3MF/profile change is required. Rebuild all consumers.

## Verification and remaining scope

Analytical flat/sloped X/Y fixtures append two disjoint finite-width later paths,
check every old canonical row/origin and new XYZ/section/dose/identity, and verify
113-bit cumulative full/current amounts. Quarter and three-quarter current-packet
points distinguish an actual half front; a different future path stays absent.
A third original surface and actual bead use the appended Lower/nominal material;
an unlaid second pass refuses that support. A further append preserves both
previous generations and retains their original protected owners.

The original native wedge appends eight .38 mm packets plus one connector,
preserving the complete original 2034-row body and first-cap journal. Independent
whole-run geometry checks the new local third-surface Lower support. Complete
cap coverage, normal/side-floor contact, exact partial-before continuation,
head/order/travel/flow, final-byte replay/export, whole-job provenance/resource
containment and physical qualification remain pending. Public export stays BLOCK.
