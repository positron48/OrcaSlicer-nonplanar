# ADR-0026: preflight the basic native path-generation domain

Status: implemented and native tests pass; independent review pending.

COMPATIBILITY requires rectilinear infill, uniform planar layers, ordinary fixed
width walls and no object-by-object order. Eight additional native discrete
rules enforce by-layer print_sequence, rectilinear sparse/internal-solid/top/
bottom patterns, disabled infill_combination and detect_thin_wall, and nowhere
gap_fill_target. Source and resolved object/volume/range settings share the
existing numeric native-enum checks; imported labels cannot redefine meaning.
Missing/wrong types and unknown native enum values remain conflicts.

PrintObject::combine_infill changes infill height; PerimeterGenerator's thin-wall
and FillBase's gap-fill paths can generate widths outside the preliminary native
adapter domain. These switches must be rejected before planning instead of
silently changing the user's profile. The tests explicitly construct an eligible
rectilinear fixture; global upstream defaults remain unchanged.

This is configuration preflight only. Classic perimeters can still generate gap
fill independently of gap_fill_target, so the future adapter must validate the
actual emitted semantic paths and reject unqualified variable-width records.
Bridge/support geometry, numeric widths, cooling/motion transforms, complete
registry coverage and guarded export remain separate unimplemented checks.
