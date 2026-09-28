# ADR-0023: canonical identity of owned native configuration

Status: implemented and native tests pass; independent review pending.

Native option serialize() is diagnostic text, not an exact identity: neighboring
binary64 values may share a rounded string, vectors/nil/percent flags and generic
enum dictionaries matter. Add canonical_json() and fingerprint() to the immutable
ResolvedConfigSnapshot. This is a config identity, not a full JobSnapshot,
compatibility decision, provenance of discarded importer keys or export approval.

Canonical schema 1 is ASCII JSON with sorted object keys, no whitespace and an
ordered options array sorted by raw key bytes. An option is [key_hex, native_type,
nullable, value]. Strings/keys/dictionary labels encode their exact bytes in
lowercase hex. Binary64 values encode all 64 bits in big-endian hex, including
signed zero and raw nullable/invalid payloads; no JSON NaN/Infinity or decimal
rounding is used. This preserves identity, never validates invalid numeric data.
Integers and booleans use JSON integer/bool forms; points/nested vectors preserve
their dimensions/order; FloatOrPercent is [binary64_hex, percent_bool].

Enums encode ["native", integer] for native typed scalars or
["generic", dictionary_or_null, integer_or_vector] for generic enums. Dictionary
entries are [label_hex, integer], sorted by label bytes. Typed enum meaning also
requires the native option key and pinned software version in the future job
manifest; this format never hashes typeid/compiler-specific names. Unsupported
native representations throw rather than fall back to rounded serialization.

The exact UTF-8/ASCII JSON bytes use the existing OpenSSL SHA-256 implementation,
exposed as a shared byte helper. Bound output to 4 MiB and option count to 4096.
An independent Python struct/json/hashlib oracle defines empty and mixed-option
fixtures before implementation. Native tests must match both bytes and digests,
exercise actual full config, mutations invisible to serialize(), nullable bits,
generic dictionary ownership, nested values and bounds. No persisted consumers
exist to migrate; future manifests must bind schema and pinned native software.
