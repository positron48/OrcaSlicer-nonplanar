# ADR-0075: exact final-byte Nominal union at packet cuts

Status: accepted for the bounded synthetic component. Parent ADR-0074. Full B12
remains open; immutable SPEC/DATA_CONTRACTS remain unchanged.

The normal terminal region of a complete actual run footprint can cross several
short underlying packets. Three-dimensional subdivision of this region cannot
remove the original coordinate allowance: native packets are about .01425 mm
long while the original query allowance is .017448769336110316 mm per axis.
Repeated shallow per-event queries therefore exhaust work/deadline without
establishing the actual continuous union. Original Lower anchors already pass.

Use the independently reconstructed exact adjacent forward-collinear runs as
an index of the original closed Nominal section union. Clip the entire query
projection at each exact final decimal packet boundary and actual partial front;
each complete longitudinal slice must fit its own final E/flow-derived nominal
dose, section, top and floor. Never average sections/doses, fill a bounding box,
extend a finite end, skip an interruption/turn or supply future material. All
positive slices together cover the complete original projection. This changes
the proof partition, not the declared material geometry or any error allocation.

Protected joined cover snapshots carry an immutable representation tag.
Nominal uses original command dose and closed finite sections without Lower
erosion. Lower keeps the original minimum-dose common-run envelope and original
strict external-end/XY/Z erosion. Upper remains unsupported by this API. The
old Lower factory/CLI remain Lower-only with their original contracts. A separate
Nominal CLI command checks the same strict six-field query. Support retains
original per-event whole near-ray emptiness, complete pre-run-only material,
separate joined Lower anchors, and exact joined Nominal normal terminals. There
is no new persisted JSON/IR/profile/3MF/cache format; existing versions stay 1.
Every internal consumer distinguishes the protected representation explicitly.

All capture/index/exact-section/partition work remains cumulative. Fresh source,
rate, material, join and support policy plus cancellation, numeric environment,
cell/depth/work/deadline checks remain through positive and negative publication.
The original all-stage CLI deadline is one second. UNKNOWN publishes no partial
certificate or witness. Original footprint/loss/error/dose/gap/margin values and
all negative cases remain. This exact Nominal proof does not qualify delivered
dose, physical bonding/contact, a measured head or the complete print job.

The unchanged native eight-packet half-prefix now has complete support PASS:
16 full-width parameter leaves, 104 cells, API work691602; separate all-stage
CLI work651623, .615 s, original one-second deadline. Independent 113-bit decimal
replay proves every old Nominal empty region, every Lower anchor/Nominal terminal,
whole affine ray bounds and full disjoint footprint partition. The original
external front/per-event Lower refusals and complete B09 exit UNKNOWN remain.

CTest target selection also replaces its overlong name regex with exact discovery
indices and verifies that rediscovery retains precisely the requested names.
398 actual tests and a real 80-long-name selection regression pass; empty,
missing, unbuilt, skipped and failed selections still reject. No tests are omitted
or made optional. This is runner selection only, without slicer behavior changes.

Full curved/stepped/cap/head/contact/route geometry, qualified dose/transforms,
B13 job/plate/resources/software/bytes/report binding, independent review,
platforms and physical work remain. Full B01–B15 IN_PROGRESS, guarded export BLOCK.
