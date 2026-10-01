# ADR-0050: Replan the complete first candidate with finite longitudinal butts

Status: locally verified; independent review pending.

## Invariant

Replanning replaces a complete prospective first candidate; it does not append
that old hypothetical material to the actual body. Retain the exact actual
body prefix, first target, fixed nominal width, transverse centres and original
boundary band. Extend the finite material butts longitudinally, reconstruct all
roof/gap/amount proofs, then require measured whole-union coverage gain and
missing-volume reduction within the original requested limits.

## Decision and provenance

The original affine hatch constructor conservatively insets both XY axes by
half width, source numerical error and the boundary band. Its first-pass material
model has flat finite butts; extending longitudinal endpoints does not widen its
transverse footprint. The new factory chooses inward represented endpoints at
ROI boundary + original band + captured hatch numerical error, and computes Z
from the original first affine surface with exact arithmetic and charged storage
error. Actual roof/section proofs still run across every complete finite width.

Create an owned replacement hatch snapshot, changing only first-pass endpoints,
reverse alternatives, projected lengths and charged numerical bounds. Whole
source target and strip quotas remain unchanged; quotas never become commanded
amounts. Preserve all later prospective lines. Reuse the complete first-layer
factory to reconstruct a fresh ledger and measure S/U/R/C/M/spill together.
Old and new candidates retain separate immutable ledgers; the target proof and
actual body stack are shared exactly. Require outward positive coverage gain
and missing-volume reduction, and the declared outside-target limit. No margin,
numerical precision, work, cell or deadline limit is relaxed.

`affine_hatch_contract_version` changes 2 -> 3: add explicit first-pass extent
provenance `CapsuleInset` / `FiniteButtInset`. Original construction always marks
CapsuleInset; only the protected replan factory may create FiniteButtInset.
A repeated end replan refuses that already changed extent. Add the internal
version-1 replan policy/result/snapshot. All current consumers are rebuilt;
there is no persisted hatch/cache/IR/3MF/profile representation to migrate.
Native original-hatch generation and existing APIs retain their behaviour.
Consumers must use owned replacement paths, not reconstruct endpoints from
original capsule insets. Tests check source/target identity and explicit geometry.

Validate the original full nested limits before reducing them; share root work
and deadlines across copying, endpoint construction and complete remeasurement.
Retain cancellation, stale-revision and rounding checks until publication.

## Boundaries

The first-layer factory measures prospective geometry. This replan does not
modify an actually laid cap prefix, qualify nozzle/head access or side/floor
contact, or approve connectors/order. Hardware working-zone margins are a
separate obligation; finite material extent is not a head-clearance certificate.
Remaining 3D voids, excess, perimeter/seam, curved/later actual support, full-head
CCD, motion/flow/order, job/plate/provenance and final-byte replay/export remain
required. The public gate stays blocked and full B01-B15 stays active.

## Verification

Positive/negative tests are added before implementation. Both first axes check
owned replacement lineage, fixed band/centres, longer finite extents, fresh
ledger and independently checked volume behaviour. Mutation, insufficient gain,
already-replanned extent, invalid nested limits, shared path/record/packet/work/
cell bounds, deadline, stale revision, cancellation and rounding refuse snapshots.
Actual native evidence and commands/XML/hashes are recorded in the milestone
report and evidence archive after final verification.
