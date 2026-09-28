# ADR-0025: require neutral native extrusion multipliers before qualification

Status: implemented and native tests pass; independent review pending.

The existing neutral-transform policy must cover the actual native extrusion
multipliers, including filament/print-wide, surface/role-specific, bridge and
brim factors. GCode::_extrude multiplies path volume by these settings; bridge
flow also affects native Flow geometry. Copying settings or hashing them does
not establish a calibrated delivered-volume contract.

Until such a contract is implemented, require exact native 1.0 for all sixteen
active-domain factors: filament_flow_ratio, print_flow_ratio, bridge_flow,
internal_bridge_flow, top_solid_infill_flow_ratio, bottom_solid_infill_flow_ratio,
first_layer_flow_ratio, outer_wall_flow_ratio, inner_wall_flow_ratio,
overhang_flow_ratio, sparse_infill_flow_ratio, internal_solid_infill_flow_ratio,
gap_fill_flow_ratio, support_flow_ratio, support_interface_flow_ratio and
brim_flow_ratio. Every filament-vector entry must be present and neutral.
The source check runs before normalization; the resolved check owns its values.
Wrong/missing types, NaN/infinity, empty or nonneutral vectors and adjacent
binary64 values fail closed without rewriting the profile. OFF remains upstream.

Inactive scarf/spiral/ironing/purge factors remain gated by their already
unsupported parent modes. Neutral factors do not qualify bridge support,
pressure advance, material delivery, motion/cooling filters or guarded export.
No global default, native flow implementation or persistent schema is changed.
