# ADR-0005 — absolute coordinates and deposition measures

Status: IMPLEMENTED for internal A03 bootstrap; native analytical tests run.
Independent A08 review remains pending. Owner: bootstrap implementation in this chat. Extends the
normative `../nonplanar/DATA_CONTRACTS.md`; does not change its requirements.

## Decision under test

Use C++17 types under `Slic3r::nptop`. Positions carry their frame in the type:
model, plate, commanded machine, physical machine and tool. Motion IR positions
are absolute nozzle-opening positions in the physical machine frame. Identity
between commanded and physical positions is an explicit, validated firmware
precondition, never an implicit conversion.

Length (mm), deposition volume (mm³), filament feed/retraction (mm) and vertical
versus normal gap remain distinct concepts. Non-finite values and invalid
dimensions reject the event. Retraction/unretraction are separate event kinds
and create no deposited material. Layer numbers are display labels only.

Primary volume definition for new cap cells is the integral of vertical gap
over owned XY area. For an affine gap on a rectangle, V is XY area times the
mean of the four corner gaps. This is an analytical primitive, not a complete
rounded-bead/contact model. Edge cells, seams and transition coverage still
require their own integration and ownership rules. Multiplication by 3D/XY
length after this integration is forbidden. For parallel affine planes,
normal gap is vertical gap / sqrt(1 + fx² + fy²).

The filament command primitive is E = V / (pi d²/4) * k, applying k once.
The meaning of k is command compensation; it does not enlarge nominal
geometric volume. k=1 is the initial analytical baseline. A06/B10 must
qualify non-unit flow and actual native writer/replay behavior before use.

## Native boundary

`coord_t` is int64_t but `SCALING_FACTOR` is mutable at the pinned revision.
Capture the scale alongside native paths while the corresponding Print state
is stable. Named conversion functions require the captured scale and reject
a different current scale. Never reinterpret scaled Z as a double in mm or
mix ZAA-relative Z with physical absolute Z. No global Orca type migration.

Conversion domain and rounding error must be explicitly tested and charged
to the transform/serialization budget. Unsupported magnitude, NaN, infinity
and scale mismatch reject; they cannot be clamped to zero.

## Material, numerical budgets and versioning

Nominal, upper collision envelope and lower support envelope use separate
types/identifiers. A typed identifier alone proves no geometric inclusion.
A04/B05 must implement and test D_lower ⊆ D_nominal ⊆ D_upper with chronology,
including deposition inside the current segment.

Numeric budgets retain import, chord, distance-query and conversion/rounding
components. Tool measurement, positioning, bead uncertainty and required
physical clearance stay separate. An unset budget cannot yield clearance PASS.

IR v1 is internal and experimental. No serializable project/cache payload or
Verified state is introduced by A03. A06 establishes final byte round-trip;
B01/B13/C06 must bind this contract version and captured scale into snapshots,
cache keys, invalidation and 3MF schema before those paths exist. Future
changes require a new ADR plus tests and migration of every consumer.

## Executed acceptance evidence

ORC-05: nonzero XYZ, both native scales, conversion bounds, changed scale and
invalid magnitude. VOL-01: exact flat/sloped cell integrals and rejection of
double slope correction. VOL-02: independent parallel-plane normal distance.
These tests do not establish a usable planner, contact model or safe export.

## Implemented numerical domain and deferred consumers

Native adapters support the two pinned scales only, XYZ magnitude <= 10000 mm
and native integers <= 10^10 before conversion to double (below 2^53). Encoding
rounds to nearest, ties away from zero; the Euclidean bound is sqrt(3)*scale/2
plus decode arithmetic allowance. The decode allowance is conservatively
sqrt(3)*4*epsilon*10000 mm, rounded upward. Consumers must charge the returned
bound with NumericBudget::require_conversion; it does not include firmware,
import, chord or measurement errors. NativeScale checks the current scale but
does not synchronize global writes: a future snapshot owner must hold Print
state stable throughout conversion. No production caller exists yet.

The affine-cell primitive accepts three corner gaps and derives the fourth,
rejecting any nonpositive corner. It assumes an affine vertical gap; arbitrary
mesh cells and certified accumulated volume error remain A05/B10 work.
Retraction v1 is stationary with explicit before/after states. Whole-plan
continuity, matching amounts and material inclusion remain later validators.
No persistent formats or cache keys change in A03; introducing a serializable
consumer requires the version binding/migration described above.
