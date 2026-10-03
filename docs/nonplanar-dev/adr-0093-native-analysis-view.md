# ADR-0093: native analysis view with serialized display adoption

Status: implemented source; execution qualification is recorded separately in
B14-native-gui.md. This does not close B14, Gate A/B or the full P2 scope.

The existing Plater worker queue runs the bundled native analysis supervisor.
A modal wxWidgets dialog loads/edits the same version-1 request as the CLI,
selects an explicit analysis mode, shows stage progress/cancellation, the public
blocked report and movement-index replay with cumulative duration intervals.
The mode overlays a copied resolved configuration; original presets and stock
Print/cache are not changed. The native planner and strict worker transport are
shared. There is no second planar engine or external UI server.

Plater's actual filament-map/full-config resolver is shared with the existing
background apply path. Current plate overrides are then applied exactly as in
BackgroundSlicingProcess::apply. Capture retains the original mode, exact source
geometry/settings/placement/annotations, exact binary64 plate origin and exact
editing bytes. A monotonically increasing dialog revision rejects edit/undo ABA.
Background work receives an owned Model/config and only communicates cancellation
and progress through atomics/the existing Job controller. Its Print is private.

Finalization runs on the main thread and recaptures the live GUI input. Identity,
revision, current native Analyzing token and original source fingerprint must
match. A successful stop of that private owner is required before showing a
report. It always stops as Unknown; cancellation stops as Cancelled. Late or
repeated callbacks discard diagnostics and cannot change a replacement owner.
Modal timer delivery drains the existing queue. Closing during work cancels and
waits for completion without blocking the GUI; forced destruction is protected
by wxWeakRef and privately owned background state.

The view displays the actual independent final-byte replay and its byte ranges.
Accumulation rounds time bounds outward; displayed JSON bounds expand one more
binary64 ulp to cover decimal rendering. Cocoa smart quote/dash substitutions
are disabled in the editor and report. The loader uses an all-files native
selector followed by the strict shared version-1 grammar; platform associations
cannot rewrite/approve the input.
XY/XZ/YZ projections render the completed prefix and selected original movement;
preview-only LOD above 10,000 rows is explicitly labelled. No raw candidate bytes,
export/download/send control, native proof or Verified credential enters the view.
Full surface/body/cap/transition/head/material/conflict/cross-section displays,
complete role metadata, time seeking and automatic domain editing remain open.
The editing JSON is an intermediate explicit input, not a replacement 3MF format.

The executable is chosen by the build and copied beside the app, installed on
Linux/Windows and included in the macOS bundle. Linux AppImage preparation also
copies the sibling worker. SLIC3R_NPTOP_LAB enables the menu; it is a feature flag,
not proof of isolation. The supported macOS development launcher enables it in
an independent app identity/datadir/cache/temp tree under its inherited offline
write-restricted sandbox. Imported profiles cannot choose the executable.

An actual startup sample found the stock cloud agent blocked on wxSecretStore /
SecKeychainFindGenericPassword despite that sandbox. Lab startup therefore skips
network-agent creation before the credential broker, using the existing no-agent
DeviceManager/UserManager fallback. Normal startup without the lab flag is
unchanged. No Keychain permission was approved; a separate datadir and denied
sockets are insufficient by themselves. The final isolated core and bundled
worker are checksum-bound to the GUI observation record.

Actual GUI import exposed missing plate metadata and a nonidentity source
transform in a stock unsliced 3MF export. These inputs refuse. Explicit synthetic
fixtures retain those negatives; an original STL import and a separately logged
identity-source fixture pass the existing source/placement audits. No production
importer, source equality rule or compatibility policy was weakened. Complete
3MF-container provenance and faithful guarded project round trips remain open.

Child geometry work remains killable and bounded by the existing worker. Parent
source/config capture, file observation and private Print::apply have cooperative
bounds, not hard interruptibility or hard host RSS containment. Their complete
limits, full source/file/software provenance, Windows/Linux GUI runtime and
native 3MF/publication still require qualification. Existing safety margins,
unknown mandatory checks, export block and all original negative fixtures remain.
