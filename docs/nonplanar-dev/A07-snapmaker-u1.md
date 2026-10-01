# A07 — first operator machine: Snapmaker U1

User selected **Snapmaker U1** on 2026-09-27 and reported a **standard head
with a nominal 0.4 mm nozzle** on 2026-10-01. These are user-reported configuration
facts, not measured head geometry or an operator_confirmed profile. Active tool,
configuration, offsets and dimensional qualification remain **UNCONFIRMED**.
Firmware/material details are not prerequisites for the current software
implementation. Physical qualification remains separate. This note is not a
printer preset or permission to print.

Official references inspected 2026-09-27:

- [U1 specifications](https://www.snapmaker.com/en-US/snapmaker-u1/specs):
  four included toolheads, toolhead offset calibration and mesh bed leveling.
- [Snapmaker firmware publication](https://www.snapmaker.com/blog/snapmaker-u1-firmware-now-on-github/):
  modified Klipper, including tool switching, probing and XYZ offsets.

Consequences for our planned scope (engineering inference): first qualification
should use one selected tool, fixed orientation and no in-job tool changes.
Docked heads, docking hardware, bed clips and other obstacles need scene coverage;
generic Klipper identity cannot stand in for the U1's actual firmware transforms.
Selection/preparation and any start/end macros must be audited separately.
The installed firmware/configuration must be obtained as a read-only operator
export; do not contact the printer or run macros to discover it.

Human evidence needed at A07:

1. Installed tool/nozzle/duct identification and photos; measured nozzle outer tip
   and full head envelope in nozzle-centered coordinates, with measurement error.
2. Scene obstructions/docks and usable travel domain; actual Z dynamics and
   firmware limits from configuration, not marketing maximum XY speed.
3. Versioned firmware/configuration and relevant macros, offsets, bed mesh and
   fade behavior; an explicit verified transform contract or rejection.
4. Material/filament calibration and residual bed deviation for the chosen
   preparation. Physical clearance, extrusion quality and repeatability require
   later operator tests after software gates, never inferred from this bootstrap.

The simulation JSONs in `tests/nonplanar/data/baseline` intentionally describe a
generic software fixture. They are not U1 settings and must not be sent to U1.

The continuation worksheet is [A07-operator-worksheet.md](A07-operator-worksheet.md),
with an entirely unconfirmed [record](A07-operator-record.json). The native
versioned scene experiment is described by ADR-0009; it has no U1 dimensions
and cannot qualify an operator profile.
