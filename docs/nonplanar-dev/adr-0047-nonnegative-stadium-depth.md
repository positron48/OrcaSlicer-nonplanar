# ADR-0047: Intersect stadium depth bounds with their nonnegative domain

Status: implemented and locally verified; independent review pending.

## Invariant

The nominal depth below a rounded section's axis top is
`r - sqrt(r*r - u*u)` for `r >= 0` and `0 <= u <= r`. It is nonnegative.
Uncoupled interval heights/radicands must not invent nominal material above
that top or below the corresponding `top - h` floor.

## Decision

Intersect every rounded nominal depth interval with `[0,+infinity)` before
returning the roof enclosure. This preserves every admitted real section,
including partial transverse footprints. Retain the existing correlated
endpoint refinement, outward arithmetic and rectangle path. No tolerance,
margin, work/cell limit, deadline or Unknown policy changes.

The same bound feeds nominal roof/integral and union/fill queries. D_upper and
D_lower remain separately defined. No public API, saved schema or snapshot
contract version changes. Public guarded export stays blocked.

## Verification and boundary

An independent variable-gap stadium lies wholly between the actual flat body
and an affine target at both current progress 0.3 and completion. Before this
change its two-cell fill budget fails; after the change both spill integrals
finish in exactly two cells and enclose the actual binary commanded amount.
A raised rounded stadium retains strictly positive above-target material,
bounded independently by its flat-core and full-width prisms.

This removes an invalidly broad interval, not a real deposited region. It does
not resolve the complete B07 contact/fill/order problem. Native fill still
requires 40960 cells / 865083 work locally; the inherited Linux 20-second
deadline failure is not declared repaired. Optimize certified roof queries
separately, keep that deadline, and require a successful Linux run.

Commands, red/pass XML, full native regressions and source/binary/output hashes
are recorded in `B07-stadium-depth-bounds.md` and its evidence archive.
