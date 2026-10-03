# ADR-0096: ordered later paths recomputed on their actual material prefixes

Status: bounded C++ software path; complete filled cap/job and independent safety
review remain pending. Scope: DepositionModel and analytical tests. No normative
model, existing fixture, IR/profile/3MF/report/worker schema or export change.

## Invariant

`plan_next_cap_sequence` owns a bounded ordered list of local later-path requests
and the exact complete first-cap-rooted material prefix. For every request it
recomputes whole-footprint actual Lower support and vertical feasibility, then
mandatory actual normal spacing, finite-width nominal roof and constant-flux dose.
It appends that protected bead, preserving every previous canonical row, before
the next request can use the resulting material. Neither an unlaid later request
nor a dose derived on the old prefix supplies future support.

The original hatch policy supplies the fixed width and each original pass supplies
its alternating X/Y direction. Requests select only an original local footprint,
support-plane candidate and current/immediately succeeding pass. The plane must
be certified against actual Lower material; it is never inferred as occupied from
the prospective surface. No pass skips, partial-before continuation or implicit
width reduction. Full layer coverage is not inferred from advancing the pass.

Copy source, caller list and limits before callbacks can mutate them. Retain all
protected bead owners and their exact before/after prefixes in a privately
constructed immutable result. Root work/cell/time bounds span support, normal,
roof, packet and append stages; nested cancellation/current predicates and packet
limits still apply. Reject stale, cancelled, exhausted or exceptional work and
poll at final publication. Failure publishes no partial sequence/material result.
Failed-bead work is bounded by its remaining child limit, but the existing bead
failure result does not expose its spent counters; sequence counters do not claim
an exact failed-work measurement. Cooperative callbacks are not hard preemption.

## Whole-union proof completeness

An intersecting continuous run can be useful for part of a union without enclosing
the complete requested cell. Its single-run solver subdivides longitudinally;
that cannot repair missing transverse material. Trying this impossible proposal
first could exhaust the unchanged cell budget before the union partitions its
domain. Use the outward nominal run enclosure only to reject that proposal. For
Lower, the exact existing loss/error-expanded query must fit the enclosure.

Keep the intersecting run as a candidate for subsequent union subdivision. A
fitting enclosure never accepts a leaf: the original complete nominal/eroded run
inequalities must still certify it. Rounded shoulders, gaps, true finite ends and
current fronts retain their original geometry. No margin, error allowance, depth,
work or cell limit is increased and no negative expectation is removed.

## Verification and remaining scope

Both axes and separate flat/shallow affine fixtures plan two sequential later
paths, including an actual nonzero-Z later path. Every old canonical row remains
exact; the second path owns the material produced by the first. Independent
113-bit whole-run/event inequalities and full disjoint plane/volume partitions
check support and normal terminal leaves; separate command sums check dose.

The original .04-mm-rise affine fixtures remain unchanged and refuse the original
alternating transverse path under the unchanged .001-mm roof-gap approximation
limit. The positive shallow fixture is a separate .004-mm-rise source, retaining
the same policy/body/width/error limits. It does not qualify the steeper case.
An independent stadium inequality refutes a shoulder box inside the nominal
enclosure, so both nominal and Lower queries must refuse it.

Requests remain local paths, not complete filled layers/cap, qualified forming
contact, seams, full head geometry, travel/order/flow or final-byte/job/export
proofs. The common native analysis controller still plans its original first-cap
candidate; later-program capture/lineage/replay integration and automatic complete
cap construction are next dependent work. Guarded export stays BLOCK and complete
B01–B15 stays IN_PROGRESS. Physical qualification remains operator-only.
