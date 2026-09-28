# B01 neutral flow and geometry transform policy

Invariant: unqualified native flow/shape compensators cannot pass the bounded
guarded configuration preflight. This adds eleven explicitly typed rules to the
native registry; it is not the complete compatibility policy or export approval.

The reviewed call sites are GCode's AdaptivePAProcessor, PressureEqualizer,
SmallAreaInfillFlowCompensator and adaptive volumetric-speed selection, plus
Print::shrinkage_compensation and native XY/elephant-foot settings. Disabled
values are required for adaptive PA and its overhang/bridge variants, adaptive
volumetric speed, small-area flow compensation and extrusion-rate smoothing.
Shrinkage must be exactly 100% in XY and Z; hole, contour and elephant-foot
compensation must be zero. Ordinary fixed material flow calibration is not
silently changed or declared qualified by this policy.

Every native vector entry is checked; an empty vector, wrong native type,
missing option, NaN, infinity or unresolved nullable sentinel fails closed.
Binary64 values are compared by their representation, accepting both signs of
zero but no epsilon or rounded string equivalence. A one-ULP departure from
100% is rejected and displayed with max_digits10 precision. Subnormal nonzero
values cannot become accepted zero through a caller's floating-point flush mode.

These checks also cover sparse model/material/volume/layer overrides through
the common model policy diagnostic. Resolved values and original presets remain
unchanged. OFF bypasses this policy and keeps the native path. The previous
custom-code registry and plate-action checks continue to run.

Three new native cases test all eleven nonneutral mutations, vector/type/numeric
edge cases and the actual Print override boundary. The initial run failed in all
three cases (exit 42). An intermediate run exposed Orca serialize() throwing on a
mixed finite/NaN vector; rejected values now produce a structured policy conflict
before serialization. The initial test compile ambiguity for an unsigned-char
nullable sentinel was fixed in the fixture; the failure log is retained.

Final macOS ARM64 Release application and selected native targets build with
exit 0. Sixteen B01 cases pass (377 assertions, NoAssertions enabled); fresh
selected CTest executes 135/135 with no skips. Six fresh OFF/ZAA captures match
pinned G-code and modal motion with the existing timestamp rule and the sole
documented resolved-setting addition `nptop_mode: off`. Commands, exit codes,
failed/final XML and source/binary hashes are in `evidence/B01-transforms/`.

Full setting-schema coverage, raw unknown-key import provenance, material/firmware
qualification, complete immutable job snapshots and the actual hybrid pipeline
remain incomplete. Wipe/retract/cooling and other trajectory controls still
require their own contracts. No Linux test pass, physical qualification or
independent safety review is claimed by the local checks.
