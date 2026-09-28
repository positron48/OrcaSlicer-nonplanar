# Snapmaker U1 — operator measurement worksheet

Status: UNCONFIRMED. This document does not contain assumed U1 dimensions.
The native A07 fixture is synthetic and must not be copied into this worksheet.
Do not connect software to the printer or run discovery macros for this task.

Record each measurement with units (mm), method/instrument, upper error bound,
date, source/photo identifier and installed configuration. Leave unknowns null
in `A07-operator-record.json`; zero means a measured zero, not missing data.
Use the existing immutable `../nonplanar/profiles/printer_measurement_template.json`
as a reference, not an automatically qualified preset.

## Installed configuration and coordinate registration

Record the selected tool, nozzle, heater, sock, ducts, sensors, mounting hardware,
modifications and material/filament. Photograph front, rear, both sides and bottom
with a scale reference. Establish origin at the opening centre on the tip plane;
+Z points into the hotend, X/Y follow fixed machine axes. Record registration error.
A nozzle hole diameter is not the outer tip diameter.

Measure the outer tip and radial nozzle profile versus local Z; measure separate
outer bounds for heater, sock, ducts, sensors and mount. Include screw heads,
wires and protrusions. Record any inaccessible or unrepresented regions and the
minimum local Z at which omitted upper components begin. Do not infer coverage
of 3D geometry from two photographs or an ordinary clearance radius.

## Scene and moving parts

Record a bounded nozzle travel domain and a scene box containing relevant bed,
clips, docked tools, docking hardware, neighbouring material and obstacles.
For moving components record axis dependence and an enclosing union across the
entire proposed domain. A single parked pose is insufficient. If this union is
unknown, leave the simulation applicability/qualification unresolved.

For any gauges, record scale error and fit tolerance separately from desired
clearance. Gauge generation is NOT_IMPLEMENTED. Future fitting must use a cold,
stationary head; do not force a gauge against the nozzle, sensor or wiring and
never leave one installed for operation. No measurement here authorizes printing.

## Firmware and material evidence

Provide an existing read-only firmware/configuration export with version/hash,
relevant start/end/tool-selection macros, XYZ offsets, bed mesh/fade/skew,
speed/flow overrides and axis limits. Do not execute macros to inspect them.
Unknown transforms remain unsupported; do not disable compensation mid-print.
Record delivered-volume calibration, filament diameter, residual bed deviation,
uncertainties and repeatability. Physical evidence remains NOT_RUN until obtained.

## Completion boundary

A name, checkbox, photo count or filled JSON is insufficient for qualification.
A later software gate must validate measurements, installed configuration,
provenance, domain and revision binding. This milestone has no operator-confirmed
profile loader and cannot grant export permission. Changes in nozzle, head,
firmware, offsets or material require a new record and invalidate prior evidence.
