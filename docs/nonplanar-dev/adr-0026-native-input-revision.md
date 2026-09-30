# ADR-0026: own exact native source inputs before Print normalization

Status: implemented and native tests pass; independent review pending.

Native ConfigOptionFloat equality is approximate. A repeated Print::apply can
retain an earlier effective value even if an incoming binary64 option changes
by one ULP. A settings-only fingerprint cannot detect that source change. Own
the guarded source before normalization and bind every dependent result to its
monotonic Print revision.

NativeInputSnapshot captures raw global/material/object/volume/layer-range
configs, original indexed meshes and properties, authoritative volume/instance
and source matrices, source offsets/file references/unit flags, volume types,
material attributes, object origin/printability, layer-height profiles, instance
ordering/printability and all four facet annotation streams. It has no native
Model pointers, mutable ConfigBase API or borrowed enum dictionaries. Arrays
and native mesh data are deep copies, reachable only through a const aggregate.
Plate action bytes are included in the private canonical identity without
execution. They remain rejected by configuration preflight.

Input schema 1 embeds exact config schema 1. Object/volume/instance traversal
order is retained, material and plate maps follow sorted native keys. Matrices
are row-major binary64 bit strings, source strings are bytewise hex. Mesh schema
1 uses domain nptop-native-mesh-v1 followed by NUL, then big-endian uint64 counts
for vertices, signed int32 triangle indices and face properties; coordinates
are exact IEEE binary32, property types signed int32 and areas binary64. Native
stats/normals/convex-hull caches are not geometry inputs. The SHA-256 identities
and JSON framing have independent Python struct/json/hashlib vectors. No
rounded native serializer is used for identities.

Limit the complete graph to 256 combined objects/materials/volumes/instances/
ranges/attributes/plate action entries, 200000 faces and 600000 vertices,
4096 layer-profile values, bounded annotation streams and 4 MiB canonical JSON.
Object SLA support/drain controls, custom brim points and cut connectors are
unrepresented and rejected. These are synchronous capture limits, not a hard
process memory/CPU containment claim. Capture failure retains no partial input,
records a blocking diagnostic and increments the revision on every failed apply.

The input comparison and revision invalidation occur under the existing Print
state mutex before native normalization (which may itself throw). Changed
source identity or diagnostics cancel background processing, invalidate native
psGCodeExport and replace the owned source. An identical successful source
preserves the revision. Switching to OFF clears the source and invalidates a
prior guarded result; repeated OFF avoids captures and revision changes. clear
invalidates the retained source. Revision overflow must fail closed without
wrapping. NativeInputBinding returns snapshot and revision together under the
same mutex; downstream ownership/callback checks must compare both against the
current job dependencies before publication.

Print settings canonical schema 2 adds input_revision and input_fingerprint.
The version bump deliberately invalidates schema-1 cache keys; retained schema-1
fixtures/evidence are historical. No persisted project/settings migration is
needed because this internal snapshot has no public project consumer. Actual
full/effective settings remain separate from raw source settings; a one-ULP
source change can change the guarded fingerprint while actual cached native
Print settings stay the same.

This boundary is not a complete JobSnapshot or export approval. Native plate
index/origin setters, GUI plate membership, original STL byte/error provenance,
software identity, tool/scene qualification, material bounds, worker context,
result-state transitions and atomic publication still require binding. Arbitrary
new settings are retained for identity, not approved by a compatibility registry.
All guarded slicing and export still fail preflight after supported checks.
