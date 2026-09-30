# ADR-0031: native body trajectories bound to declared material

Status: implemented bounded adapter; independent review pending.

Keep the complete owned PlanarBodySnapshot and reconstruct every ordinary native
planar segment. Preserve source region/path/segment and event references. Explicitly
translate BuildPlate positions into MachinePhysical positions; require the stored
scaled points and layer/origin to produce exactly the retained millimetre points.
Validate native coordinate bounds before signed subtraction. Native role/Flow
checks reject unsupported bridge or altered flow, rather than infer support.

Orca Flow::mm3_per_mm rounds its rounded-rectangle area to float. Preserve that
commanded area; derive effective material width from A/h+(1-pi/4)*h rather than
silently treating the native float width/height as an exact different area. A
binary64 event volume also charges its rounding in the derived interval. Retain
native width/rate and nominal native volume in the parent body; bind each width
and volume reconciliation bound in the references. Apply no flow ratio again.

The material numerical coordinate allowance adds inherited partition error,
native origin/path conversion, explicit translation arithmetic and half of width
reconciliation. Their sum must fit 0.05 mm. Physical inner/outer assumptions stay
separate. This adapter has no measured hardware or delivered-flow qualification.

Retained native enumeration order supplies a diagnostic sequence, with explicit
straight connecting Travel records. It is not the native exported G-code order.
The initial state is Ready, without invented retract/unretract. Explicit speeds,
acceleration and support/contact reference IDs are structural reconstruction
inputs, not proofs. No entry, connector, limit or local contact is qualified here.
B09 must provide a legal verified full order; B10 must qualify motion/flow limits.

Bind parent body/material identities, physical plate translation and every native
association with a bounded domain-separated record chain, schema 1. The body
parent retains original plate/config/partition provenance. Cancellation, deadline,
stale revision, changed arithmetic and aggregate limits reject without a partial
accepted snapshot. Geometry stays owned when caller handles vanish in callbacks.

The native positive uses the actual reserved flat body and all its segments;
independent long-double flow/width and prefix tests check observed data. Negative
mutations include mismatched native/scaled points, extreme integer coordinates,
unsupported roles/flow, exhausted aggregate budget and cancelled/stale/late work.
Cross-language synthetic hash vectors test encoding only. Dense coverage, seam,
whole-job/worker binding, hard containment, CCD/contact and export stay pending.
