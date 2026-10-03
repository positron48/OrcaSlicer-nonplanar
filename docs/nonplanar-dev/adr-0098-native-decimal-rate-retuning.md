# ADR-0098: native decimal rate retuning

Date: 2026-10-03. Implementation accepted; independent safety review pending.
Scope: B10/B11 full-stop simulation serializer. Full B01–B15 remains active.

The original later connector passes planning on binary64 geometry but its
native decimal Z changes the axis ratio. M204 S85.530745 produces Z acceleration
50.000000410997415293 on the final connector, exceeding the unchanged Z50 limit.
ADR-0097 retained this refusal and used an explicit execution reduction to40 for
its positive fixture. General serialization must account for its own rounding.

Before emitting bytes, bound the native formatter's exact integer decimal
lattice with the existing production interval arithmetic. Integer units and
integer differences are exact in the existing bounded formatter domain. The
initial pose is the actual quantized binary64 position; later poses are the
commanded decimal coordinates. E uses the one original V→filament conversion
and its actual decimal lattice. No second flow or slope correction is applied.

Reduce the original global acceleration and each original speed ceiling only
where the resulting actual XYZ/Cartesian or CoreXY drives/E/Q require it. The
event-frequency bound remains conservative. Format F and M204 downward after
these bounds. Preserve XYZ/E, order, pressure state, dwell, barriers, journal,
source ownership and the original coordinate-conversion budget. No trajectory
is dropped, shifted or resampled to make rates pass. Excess cross-section,
geometry, dose or unsupported firmware cannot be repaired by slowing down.

The bounded preflight shares cumulative work, deadline, cancellation, current
material/scene/policies and arithmetic-environment guards with emission and
publication. It is a producer calculation, not a certificate. The independent
exact-rational final-byte verifier remains unchanged and is still mandatory.

The syntax, semantics and policy version1 remain unchanged. This is a correction
to the implementation of existing limits, with compiled source identity binding
the new executable. No preset, profile, 3MF or normative-document migration is
introduced. Complete software qualification remains UNKNOWN.

Original100 later analysis now emits M204 S85.530744. The original unsafe bytes
and a restored unsafe header are independently refused under the original
policy. Positive mirrored Cartesian/CoreXY and coarse XYZ/E fixtures cover axis,
drive, filament acceleration/speed and Q. Original margins, negative fixtures,
limits and full17 registry remain. Export stays BLOCK.
