# ADR-0008: bounded native serialization experiment

A06 uses a fresh native GCodeWriter per candidate, with relative E and zero
writer XY offset. MotionEvent coordinates already include placement and are
machine-physical millimetres. Commanded coordinates equal physical coordinates
only under the explicit identity-firmware prerequisite. Nominal layer labels
are annotations; sequence_index must match input order and IDs must be unique.
IR version 1 is unchanged; no persisted format, cache or production consumer
exists for this experiment.

The candidate is an in-memory string beginning G90/M83, under a trusted initial
position and millimetre-unit state. No homing, priming, firmware changes or user
macros run. The low-level extrude_to_xyz formatter emits both deposition and
straight travel (force_no_extrusion), with set_speed for each event. This avoids
travel_to_xyz's implicit hop and separate speed policy. No GCode::_extrude/ZAA
code is reused. Acceleration is a prerequisite of the simulation; enforcement,
axis limits and all production motion planning remain unimplemented.

Deposition E is V/(pi*d*d/4)*k once. Retraction/unretraction are rejected until
an independently tested stateful adapter exists. The bounded experiment allows
at most 10,000 events and coordinates within +/-10,000 mm; these are numeric
limits, not a printer work envelope. Positive E below 0.00001 mm is rejected.
Writer decimal resolutions are 0.001 mm XYZ/F and 0.00001 mm E; comparison uses
half-step bounds plus 1e-6 mm XYZ/F and 1e-8 mm E arithmetic allowances. These
must be incorporated into a future geometric error budget before export.

src/nonplanar_verify/Replay.hpp is a separate STL-only parser. It imports no
planner, writer or GCodeProcessor. It restores modal XYZ/F and relative E from
actual text; accepts only G90, M83 and G1; rejects comments, unknown words,
commands, duplicate fields, nondecimal/nonfinite values and pure-E moves.
Millimetres and identity transforms are external prerequisites, not inferred
from omitted commands. The parser returns observations, never a safety PASS.
Test-side comparison independently evaluates analytic XYZ and E; production
manifest comparison, geometry/material replay, standalone executable, hash
binding and publication gate remain later work.
