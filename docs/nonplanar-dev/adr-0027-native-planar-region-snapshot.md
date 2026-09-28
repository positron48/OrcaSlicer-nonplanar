# ADR-0027: bounded immutable native planar-region semantics

Status: implemented and native tests pass; independent review pending.

Capture one actual native LayerRegion's perimeter/fill entity trees before
G-code serialization. Preserve collection/loop/multipath hierarchy, reverse/sort
flags, loop roles/inset indices, captured NativeScale, source scaled points, native layer identity/height/print Z and each
ordinary path's role, fixed width/height and mm3_per_mm. Convert points through
NativeScale into an explicitly supplied build-plate translation, with outward
coordinate-error and native length/volume intervals. Native volume describes
the unmodified path's declared mm3_per_mm, not physically deposited material.

Capture every borrowed field before callbacks; no native pointer survives in the
immutable snapshot. Bound nodes, points, nesting and elapsed time; late cancel,
stale revision, unsupported rounding or any unsupported path drops the entire
candidate. Preserve group membership instead of flattening away loop and order
constraints. This collection order is not a final motion order.

Reject bridges, gap fill, support, ironing, sloped/contoured/unknown native types,
arc metadata, nonzero point Z, disconnected groups and variable-width groups.
The caller owns/synchronizes LayerRegion during the bounded initial capture.
Only ordinary linear planar path semantics are accepted, with no export route.

Predefined independent oracle: scaled (0,0)->(3,4) has length 5 mm and native
volume 0.625 mm3 at mm3_per_mm=0.125. Native body slicing provides positive real
perimeter/fill data; mutations exercise unsupported records and owned lifetime.
Body/cap partition, full-object/plate provenance, dense coverage, ordered material
replay and transition planning remain separate requirements.
