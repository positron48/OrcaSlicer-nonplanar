# ADR-0025: retain one owned native Print settings snapshot

Status: implemented and native tests pass; independent review pending.

The config-only boundary in ADR-0013/0023 owns individual resolved configs, but
Print preflight previously discarded each after examining its first conflict.
There was no retained aggregate of the actual native regions, counts and plate
frame. Downstream analysis must not rebuild these settings from mutable presets.

capture_print_config(Print) synchronously captures the full native config, the
effective native PrintConfig overlay (including filament overrides), every
resolved PrintObject/PrintRegion config, object/instance counts, native plate
index/origin, the retained pre-normalization input diagnostic and the source
model-policy diagnostic. Each resolved config preserves the existing native
full -> native PrintConfig -> object -> region precedence. An OFF override cannot downgrade an enabled
full job. Configs share const owned storage with their policy decisions. OFF
without a retained input conflict returns null before capture, including when
the plate state would be unsupported for guarded analysis. The caller must own
or synchronize Print throughout capture; this adds no concurrent read or lock.

The region map uses Print object ordinals and native print_region_id values,
in traversal order. It identifies settings consumers inside this Print capture,
not source triangles, model geometry or persistent object identities. Limit
the capture to 256 regions; canonical output retains the existing 4 MiB bound.
An invalid negative plate index, nonfinite origin or origin outside +/-10000 mm
blocks guarded preflight. Capture/encoding failure cannot enable export.

Canonical settings schema 1 sorts JSON object keys and embeds each exact config
schema 1 object. Fields: full_config, input_conflict_hex, instance_count,
model_conflict (null or [key_hex,value_hex,reason_hex]), object_count, plate_index,
plate_origin_mm (three authoritative binary64 bit strings), regions
([object_ordinal,native_region_id,config] entries), resolved_print_config, schema. No rounded numeric
serialization, addresses or platform-specific type names are used. The whole
ASCII JSON uses the existing SHA-256 implementation. Independent Python
struct/json/hashlib vectors precede the native encoder implementation.

The native Print block consumes this captured preflight. It preserves the early
retained input-conflict rejection and returns the existing not-implemented
reason after successful settings checks. This identity is neither a complete
JobSnapshot nor a revision/worker token: source geometry/transforms, GUI plate
membership, tool/scene/firmware/material evidence, software revisions, invalidation
and publication still need their own binding. No persisted consumers or project
schemas exist to migrate; the pinned native software contract remains necessary
for interpreting config enum keys. Do not serialize this settings snapshot into
public diagnostics or project payloads: native configs can contain private data.

Native ConfigOptionFloat compares approximately, and Print::apply() can retain
the prior cached value for a one-ULP incoming change. Tests distinguish that
actual cached Print state from a fresh Print containing the neighboring full
config value. This boundary preserves exact values that Print actually retains;
it does not recover discarded incoming values or provide source-input invalidation.
The future job boundary must own the inputs before native apply/normalization.
Native PrintConfig also applies filament retraction overrides separately from
full_print_config(); tests require 0.8 mm full retraction and 1.25 mm effective
retraction to remain distinct and require effective values in region preflight.
