# ADR-0042: clipped nominal union and multiplicity excess

Status: bounded geometric primitive implemented; target-fill reconciliation,
full B07 and independent safety review pending.

ADR-0041 supplies first-centerline amounts. Summing these amounts does not measure
their occupied union: adjacent rounded beads overlap. Add internal material-union
contract 1 within DepositionModel, with geometric/native tests and execution
records. Do not change IR, material serialization, projects, profiles or export.

For an owned laid prefix and a finite positive physical XYZ box, return intervals
for U = measure(union of nominal beads), S = sum of individually clipped bead
volumes, and R = S-U. R is multiplicity excess: three coincident beads give 2V,
not the 3V sum of pairwise intersections. All three whole-domain interval widths
must meet one requested mm3 precision before the private immutable snapshot is
published. Future material is absent; the current G1 contributes only its actual
fraction. Source, domain and limits are captured before callbacks. Diagnostics
on UNKNOWN may retain provisional bounds, but cannot provide a snapshot.

Reuse exact convex XY clipping, outward nominal section intervals and exact
rational sums. An adaptive XY partition bounds vertical interval unions. A
possibly present profile supplies an outer interval; only whole-footprint
coverage supplies an inner interval. At each height, multiplicity excess is
max(count-1,0), which is monotone under inclusion of each profile. Integrating
inner/outer counts therefore bounds R independently of subtracting two loose
totals. Intersect U and R bounds with S-R and S-U respectively.

For axis-aligned packets with the same centerline, color longitudinally
overlapping finite spans into separate chains. Each chain has at most one active
profile almost everywhere. Its outer envelope can bound one profile count; its
inner common vertical interval is usable only after exact finite longitudinal
coverage and whole transverse coverage are proved. Missing packets never bridge
a gap. Closed butt planes have zero volume; this does not change point/contact
boundary semantics. Potential chain continuity is only a split-direction hint.

When exactly two chains guarantee a positive intersection over an entire XY
rectangle, their actual intersection height is concave transversely at every
fixed longitudinal coordinate. Each fixed rounded/rectangular section is convex,
and intersection with the other section and physical Z window remains convex.
Hermite-Hadamard bounds its transverse mean by endpoint trapezoid below and
midpoint height above. Outward envelopes over every possible longitudinal packet
bound these three values. This is a concavity-based integral certificate, not
sample-based acceptance. Other cells retain the general interval bounds.

Preserve slope correlation by the unit-Jacobian shear q=z-gx*x-gy*y, chosen along
the first active bead. Physical Z limits become conservative inner/outer q
windows over the exact XY polygon. No material is replaced by a supporting plane,
and there is no 3D/XY volume multiplier. If conservative complete-bead boxes all
lie inside the requested XYZ domain, S is the exact sum of binary commanded
amounts times actual fractions. Otherwise integrate every individually clipped
bead. A single constant-gap axis-aligned rectangle is clipped exactly at its
affine top/bottom and original Z planes, reusing polygon moments.

Rounded shoulder bounds preserve correlated height endpoints. With fixed area A,
radius r=h/2, core=A/(2h)-pi*h/8 and u=abs(n)-core, depth is
r-sqrt(r*r-u*u) outside the flat core. In the admitted A>pi*h*h/4 domain,
u'=A/(2h*h)+pi/8>1/2. Since r-sqrt(r*r-u*u)<=u, depth increases with h and abs(n).
For wholly covered transverse intervals, correlated endpoint heights bound depth;
outward A/pi bounds remain. Flat-core depth is exactly zero. Other queries retain
the general outward radicand bound. Existing material and roof suites exercise
these shared bounds; constant-height polygon integrals skip unused moments.

Defaults remain inherited: 0.001 mm3 precision, 4095 cells, depth 32, 200000 work
units and one-second cooperative timeout. Hard ceilings are 65535 cells, depth
32, 2000000 work units, 200000 source records and 30 seconds. Preparation,
candidate/profile work, concavity evaluations and callbacks are bounded. Stale,
cancelled, exhausted, nonfinite, invalid or unsupported arithmetic publishes no
snapshot. Cached immutable coefficients/AABBs reduce repeated computation, not
geometric margins or error bounds. Exact source and binary hashes identify this
implementation; no persisted union cache exists to migrate. Future cache keys
must bind contract/software version, prefix, domain, precision/work policies,
revision, material parameters and error provenance.

This measures declared nominal geometry. It does not certify the physical bead,
actual finite-width floor/contact, target fill or missing-volume distribution.
The native test's connector travels are a declared geometry ledger, without
motion/order approval. Target-volume reconciliation, perimeter/seam, subsequent
actual support, full-head CCD, segment/flow limits and final-byte replay/export
remain required. The public guarded-hybrid gate remains closed.
