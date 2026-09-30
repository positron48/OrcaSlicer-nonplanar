# ADR-0029: private native whole-body semantic adapter

Status: contract fixed for implementation; independent review pending.

Consume the owned VolumePartition result, its original native configs and revision.
Construct a private native Model from the body mesh already placed in build-plate
coordinates; do not re-center its volume, re-apply original transforms or alter the
source scene. Clone source object/volume/material configs, retaining their owners
for enum dictionary lifetime. Reject painted derived geometry and custom source
layer profiles until their native semantics can be preserved.

Resolve and audit guarded source settings with native Print::apply before entering
the ordinary planar engine. Only the private derived model switches nptop_mode to
OFF, while the original source remains guarded. The guarded resolved settings and
actual executed full/Print/object/region configs remain separate owned identities.
Native validation and processing run with the explicitly non-BBL body engine;
unsupported policy conflicts never disappear through the private OFF switch.
The private engine uses plate-local coordinates and an identity instance; the
original selected plate stays bound through the partition. Generated support,
skirt or brim paths reject until every such extrusion can be retained.

Decode object origin from the native PrintInstance shift using NativeScale. Capture
every native layer/region through the existing semantic adapter, preserving height,
Z, hierarchy, roles, widths, mm3_per_mm, scaled/physical coordinates and nominal
volume intervals. Empty/unsupported regions reject; never silently omit native
paths. Aggregate entity/point/layer/region and 4 MiB canonical limits apply. The
body fingerprint binds partition, guarded and executed configs, origin decoding,
all native semantic data, revision and schema 1. No MotionEvent v1 migration.

This adapter runs in an isolated native worker or serialized host tests. Model
construction/config timestamps in the pinned native engine must not race GUI
edits. Status callbacks serialize cancellation/revision polling and throw native
C++ exceptions; stop checks bracket opaque native processing. Hard process RSS/
deadline containment remains a worker integration requirement. Existing source/
placement and native path error bounds remain distinct; full native slicing error
qualification is not inferred from this nominal snapshot.

Sparse source density stays sparse. Native internal bridges produced by the
current sparse fixture remain unsupported by the existing region contract and
reject; those negative role tests remain unchanged. These unordered nominal paths do not qualify
dense under-cap support, bead-boundary seam, material evolution or ordered G-code.
B05 material reconstruction and B04 coverage/refusal still precede planning/export.
Only exact bed-contact body geometry and the existing bounded planar role domain
are supported here. Public guarded slicing/export remains blocked.

Positive tests slice all body layers, observe real floor paths under the reservation
and surrounding roof/walls, and reject restoration of paths into its strict
interior above the floor. Negatives cover policy hidden by an OFF switch, stale/
cancelled/late work and aggregate limits. Independent review and OFF/ZAA comparison
remain required; host tests never establish physical qualification.
