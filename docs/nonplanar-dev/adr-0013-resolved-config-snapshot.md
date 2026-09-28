# ADR-0013: own resolved native configuration before policy evaluation

Status: bounded implementation; local validation recorded in B01-snapshot.md.

The B01 preflight previously retained serialized diagnostic values only for the
checked subset. Those strings neither contain the entire resolved config nor
preserve every binary64 value, nullable state or numeric type. Downstream work
must not reconstruct its configuration from that map or reread mutable presets.

ResolvedConfigSnapshot captures all keys already present in a native ConfigBase.
It uses the pinned native option clone implementations, preserving exact values,
concrete types, nullable vectors, nested groups and unknown keys. Generic scalar,
vector and nullable enum options borrow dictionaries in upstream clone(); this
boundary additionally owns those dictionaries. A missing scalar dictionary rejects capture with
ConfigurationError, because its native serializer dereferences that pointer.
Native enum vectors explicitly permit null maps, including extruder_type in
FullPrintConfig; their null state and raw values are retained without inferring
an approved meaning. Static typed enum definitions are immutable tables in the
pinned binary.

A const shared storage owns the cloned options and dictionaries. Only the const
ConfigOptionResolver lookup and a copied key list are public; no DynamicConfig
or its const iterators containing mutable unique_ptr targets escape. Copying the
snapshot shares the same immutable storage. Returned option pointers remain
valid only while a snapshot handle owns that storage. This is an API ownership
contract, not protection against deliberate const_cast or arbitrary third-party
ConfigOption subclasses with broken clone implementations.

resolve_policy captures guarded configurations and performs every subsequent
preflight read against that capture. Its PolicySnapshot retains the complete
native config alongside the existing diagnostic subset. OFF returns without a
full capture, preserving its stock path. Capture does not serialize all options
or log sensitive settings. It does not reject unknown keys as audited policy;
that separate compatibility requirement remains open.

The caller must own or synchronize the source during synchronous capture. Native
Print validation already supplies a local resolved object/region config; this
change adds no parallel access or background worker. Allocation failure or a
capture exception cannot grant export. Hard input-size/memory limits, model and
plate provenance, revision ownership, whole-job cancellation, canonical JSON
and fingerprints are not implemented by this component. No complete JobSnapshot,
new persistence schema or approved export is introduced.
