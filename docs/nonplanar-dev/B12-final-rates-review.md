# B12 final rates: separate author critical review

This reviews code and observed evidence separately from implementation notes.
It is an author review; independent safety review remains pending.

The headless target links an independent library, Boost headers and JSON. Its
decision code has no planner/writer/GCodeProcessor dependency. Fixed grammar
replay admits the commands; a second exact scan decides rates from the original
decimal bytes. Its duplicate byte walk is intentional: using the parser's rounded
double positions as the arithmetic oracle would undermine the boundary proof.

Squared min-peak comparisons use OR, not AND: either the feed ceiling or the
short-segment triangular ceiling can constrain speed. Drive coordinates must
use dx+dy/dx-dy for CoreXY; Cartesian fixtures independently distinguish them.
The full-step acceleration and E constraints remain separate. Position limits
cover the whole straight segment by convexity, but establish no clearance.

Pi has an explicit alternating-remainder enclosure. Volume lower/upper bounds
are used in their proper directions; an uncertain limit overlap refuses. k is
used only to reconstruct nominal volume from final commanded E. No calibration
of actual delivered material, shoulders or pressure advance is inferred.

Floating sqrt/conversion seeds never decide PASS: exact comparisons bracket
them. Strict flags, rounding and gradual-underflow guards are required; overflow
or uncertain positive speed refuses. Work checks also bound any outward seed
adjustment. Fixed-size pi work is small and bounded; allocation and malicious
callbacks are not contained by a hard process CPU/memory sandbox.

Ideal rest-to-rest time includes both phases. The cadence test first exposed
using peak/length in place of full event time. Correcting that formula changes
this new verifier only; the original generator's stricter cap and every original
motion/material margin remain. Actual firmware delays/time still require a
different applicability model. The native fixture's pressure/dwell coverage is
absent, so those are proved only by separate analytical and CLI fixtures.

A newly authored final-pressure negative initially imposed an incorrect end
condition. ADR-0069 explicitly permits ending retracted. Retained red logs show
the error and the new positive exposes the accidental regression. Exact pressure
restoration and final debt reporting preserve the original allowed state and
cannot add phantom material or an unrequested restoration. Original negatives
and source admission are untouched.

Private const snapshots preserve bytes/policy/state before callbacks. Native
mapping retains all original limits and cumulative work, and checks material,
scene and both policy revisions through final publication. Changes after return
still require new verification; no cached report or GUI/export integration is
implemented. Unsupported source enum mapping is unreachable through protected
plan construction; the independent public API explicitly refuses unknown enums.

The CLI rejects duplicate/missing/unknown keys, nonregular/oversized inputs and
axis arrays of incorrect size. A red fixture proved JSON's default array
conversion silently ignored a fourth coordinate; explicit size admission fixes
that new CLI boundary. Reports keep job UNKNOWN and export false even on component
PASS. A report with matching counts is not a job hash or a geometry certificate.

Native rounded source recapture retains all original rows, all seven head parts,
Z=0 tip and .01 mm margin. Its extra conversion allowance is uncertainty, not
clearance relaxation. Original B09 native exit stays UNKNOWN without route
snapshot. Complete cap/access/contact work, final-byte material/support/dose,
full prolog/end/filter/job binding and physical evidence remain required.
