# ADR-0048: Evaluate interior constant-section nominal roofs without XY clipping

Status: locally verified; Linux and independent review pending.

## Invariant

For a bead with constant axial Z and constant gap, nominal roof height depends
only on normal distance from its axis. If the complete query polygon is strictly
between the start and current/full finite butt, its normal projection encloses
every relevant section. Exact XY clipping is unnecessary for that height bound.

## Decision

`nominal_roof_bounds` projects the full polygon once. Use the direct section
query only when axial Z and gap endpoints are equal and outward longitudinal
bounds lie strictly inside `(0,current_progress)`. First exclude a normal range
wholly outside the actual nominal half-width, including rectangular sections.
Compute the area from actual commanded volume and XY length, not declared width.
The existing rounded shoulder formula remains active; the section is not flattened.

A possible upper roof uses the complete normal range. Only a section covering
the whole transverse polygon may raise the guaranteed lower bound. All finite
end contacts, sloping Z, varying gaps and uncertain longitudinal bounds retain
the exact footprint-clipping path. Both paths preserve the same actual completed/
current prefix and omit future material. Query work, deadlines, error budgets,
geometry margins and Unknown handling stay unchanged.

This is private kernel evaluation, not a new geometry/model/API. No contract
version, persisted schema, fixture, profile or original normative document changes.
D_upper/D_lower definitions and public export policy remain unchanged.

## Verification

Four analytical cases cover actual circle-segment spill at two precisions,
current/full cap prefixes, future raised material absence, reverse diagonal
current body support with both cap axes, a higher rectangular neighbour outside
the footprint, source mutation, cancellation, stale revision, rounding and
exhausted shared budgets. Independent prism/circle integrals enclose the results.

The existing native rounded fill case keeps its 40960 cells / 865083 work and
volume intervals; measured macOS fill time is 16.01 seconds. Its 20-second limit
stays fixed. Successful Linux execution is required before claiming deadline
repair. Independent review, complete contact/order/export and full B01-B15 remain
open. Commands, XML, platform/timings and hashes are in
`B07-constant-section-roof.md` and its evidence archive.
