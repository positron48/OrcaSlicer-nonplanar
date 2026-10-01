# ADR-0054: Prove local joins with common lower-material volumes

Status: locally verified; independent review pending.

## Invariant

A local join requires a positive-volume box continuously inside both selected
`D_lower` beads of the same immutable prefix. Coincident axes, a point or a
shared butt plane do not prove joining material. Honour original inner/numerical
losses, finite ends and the actual current fraction; future records are absent.

## Decision

Add protected material-join contract 1. `find_material_join` owns the source,
requested domain and limits, and returns the selected record indices, one common
box and its exact outward volume interval. Its volume is a lower witness for
overlap, not the complete intersection measure. Failure provides no certificate;
it does not assert disjointness. Require a strictly positive requested box volume
(default 1e-6 mm3), with shared bounded work/cells/depth/deadline and callbacks
through publication. Reuse the original continuous `piece` predicate over the
whole box in the Lower representation, rather than sampled vertices or nominal
material. Existing material and prefix contracts retain their meaning.

For `assess_first_cap_joins`, bind the exact original candidate ledger and its
packet-to-path ownership. Request all four corners, including the closing seam,
and both ends of every interior owner. Search relevant packet pairs together
with breadth-first cells. Original eroded-butt bounds prune impossible slivers;
transverse enclosures only prune and never certify. Offer central boxes to avoid
spending the entire budget on open butt boundaries. These proposals have the
same continuous predicate and count against the same cell/work limits. Retain
the original complete search cells; never publish a partial join list.

This is additive: no changes to actual paths, amounts, nominal union/fill,
inner/outer models, contact exceptions, persisted formats or public defaults.
All consumers are rebuilt. The declared contact-model ID remains provenance;
it is not physical qualification. Independent review remains required.

## Native limitation and next dependency

The unchanged native wedge has a positive upper-corner common lower box of
approximately 3.44447e-5 mm3. Its full candidate refuses corner paths 2/3. An
independent 113-bit finite-extent calculation confirms that all nine long-side
packets intersecting the low end's nominal enclosure have length at most twice
their inner XY/numerical loss. Their separate lower sets are empty. Larger
nominal overlap or relaxed search limits cannot fix this model limitation.

Keep this native negative and the original 0.4 mm cap, target, actual body,
0.75 mm ROI, all packet/volume/width/gap budgets and material losses. A subsequent
continuous-run lower reconstruction must retain actual varying packet amounts,
sections, real finite ends, current/future semantics and uncertainty across
internal subdivisions. Do not remove end erosion from independent events or
silently reinterpret the original material model.

Local join witnesses do not prove an entirely connected lower layer, internal
packet continuity, acceptable total excess, bonding to the original body,
volumetric planar/nonplanar seam, remaining 3D fill, later support, legal head
motion/contact/order/flow, physical bonding or export permission. Full B01-B15
remains active. B07-material-joins.md records exact evidence and failed runs.
