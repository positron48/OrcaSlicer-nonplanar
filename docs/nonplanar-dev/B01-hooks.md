# B01 explicit custom code policy

Invariant: the bounded guarded configuration preflight cannot accept imported
custom G-code or post-processing scripts as qualified planner input. The same
native Print diagnostic also rejects plate actions before guarded processing or
publication. OFF returns before this new policy and leaves user data untouched.

The source audit followed `PrintConfig.cpp` definitions and native `GCode.cpp`
header/start/end, layer, timelapse, clumping, object, filament and role-change
hooks, plus `ProcessLayer::emit_custom_gcode_per_print_z` and
`Model::plates_custom_gcodes`. These are separate from the earlier post_process
check. The current global output block already prevented publication; this
change fixes inaccurate preflight acceptance and provides exact diagnostics,
not a demonstrated exploit of an allowed export.

`custom_code_policy()` is an explicit native machine-readable registry of 17
keys and expected native types. Scalar strings must be exactly empty. Filament
vectors may contain only empty strings; every entry is checked, including an
inactive second entry. A post-process list must have zero entries, so even an
empty script entry is rejected. Whitespace and comment-only code are unqualified;
no placeholder expansion, macro interpretation, shell or external code runs.
Missing fields and wrong native types fail closed. The snapshot retains the
resolved serialized values without changing their source.

An unknown textual resolved setting whose name contains `gcode` is rejected even
when empty, pending an explicit registry audit. This limited detection does not
cover all new trajectory-affecting settings or keys already discarded by an
importer's forward-compatibility substitution. ORC-35's full registry remains
incomplete. A source-based/native definition inventory guards this registry's
coverage of the pinned textual code hooks.

Sparse material, object, volume and layer-range source overrides are inspected
with the same rules even when a field does not belong to the resolved native
PrintRegionConfig. A discovered test failure demonstrated why relying on only
resolved fields would miss a misplaced object code hook.

All six native plate action types (including Unknown) are rejected when any
plate contains an item. This intentionally includes inactive plates in the
initial single-plate domain. The diagnostic identifies the plate and item count;
it does not expand or execute its text. Native Print::apply normally refreshes
only the active plate. A retained failing integration test exposed stale inactive
plate data. Guarded mode now cancels dependent work, invalidates cached export
and copies the complete plate-action map when it changes. Clearing actions
removes the diagnostic but still reaches the unimplemented pipeline block. OFF
preserves the original synchronization and native actions.

macOS ARM64 Release application and selected test targets build with exit 0.
Five new cases plus eight existing B01 cases pass (13 cases/286 assertions,
NoAssertions enabled). Fresh selected CTest executes 132/132 with no skips.
Six fresh OFF/ZAA CLI captures match the unchanged pinned G-code/motion baseline;
the sole accepted resolved-schema addition remains `nptop_mode: off`.

Three failed iterations are retained: the initial preflight/diagnostic failures,
the misplaced object-hook failure, then stale inactive-plate evidence. Each was
fixed at its actual native boundary without weakening the assertions. The final
build and verified run, all case names and exact commands/exit codes are in
`evidence/B01-hooks/`; raw logs and G-code remain under build/nonplanar-evidence.
No imported code, external script, printer connection or physical print ran. Full hybrid
planning, known qualified prologue/epilogue generation, complete settings/model
snapshots, Linux/Windows execution and independent review remain pending.
