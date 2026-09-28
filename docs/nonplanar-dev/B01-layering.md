# B01 native uniform-layer source boundary

Invariant: the bounded guarded domain cannot silently consume custom layer
profiles or layer-height range overrides stored outside resolved PrintRegionConfig.
The source policy rejects a nonempty ModelObject::layer_height_profile and any
layer_config_ranges entry containing layer_height. It identifies the object and
source container without serializing or normalizing unsupported numeric data.

Native PrintObject::update_layer_height_profile may replace invalid profiles or
create a profile from the range table; PrintApply invalidates slicing on profile
timestamp/range changes. This policy inspects the retained model source before
those slicing transformations. Even a custom flat profile is unqualified in this
initial domain. Other range settings retain the existing resolved/source checks;
the policy does not erase them or claim they are all qualified.

Three new cases first failed (exit 42, three failed assertions). After the fix,
custom variable/flat/invalid profiles and ordinary/NaN height-range overrides
are rejected without modifying their sources. Removing the input restores the
bounded preflight. The native Print integration demonstrates cache invalidation
for both profile and range edits, diagnostic propagation, slicing refusal and
preservation of the user's profile in OFF. No PrintApply change was necessary.

Final macOS ARM64 Release app and native targets build with exit 0. Twenty-six
B01 cases/2940 assertions pass with NoAssertions enabled. Fresh selected CTest
executes 145/145, with no skips/disabled cases. Six OFF/ZAA comparisons match
stock G-code and motion; only the existing timestamp rule and nptop_mode=off
schema addition are permitted. Source/binary hashes, actual commands, exit codes,
failed/final XML and baseline manifests are in evidence/B01-layering.

This enforces the source portion of the initial uniform-layer restriction; it
does not implement body/cap layering, material qualification, full compatibility,
a Linux pass or the hybrid pipeline. All guarded export remains blocked.
