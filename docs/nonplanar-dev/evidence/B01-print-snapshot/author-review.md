# Author review of the native Print settings aggregate

Reviewed the final source diff after the effective PrintConfig correction.

The snapshot owns config options and generic enum dictionaries through the
existing const storage. Region/config-policy pairs share the same owned options;
no PrintObject, PrintRegion, Model or preset pointer escapes. Plate/count/diagnostic
fields are copied. This requires synchronous caller ownership of Print, as the
previous preflight did; no unsupported concurrent capture is introduced.

The base for region resolution includes the actual PrintConfig overlay, which
PrintApply computes separately for filament retraction overrides. A real native
test checks distinct full/effective retraction lengths rather than assuming
full_print_config is sufficient. Full -> PrintConfig -> object -> region
resolution preserves region precedence and explicit job-mode protection.

Canonical framing uses exact config schema 1, authoritative plate bits, ordered
region ordinals/IDs, copied diagnostic bytes and counts. Independent predefined
Python vectors cover framing and SHA-256. Adjacent input values can be discarded
by native approximate comparison; the snapshot does not invent them afterwards.
That native boundary is an explicit blocker for claiming whole-input provenance.

Native OFF returns before config capture. Guarded plate-domain, region-count or
capture exceptions produce blocking reasons. A successful settings preflight
still returns the existing not-implemented block: no PASS or export path was
added. The 4 MiB aggregate encoder limit is separately tested; it is not a hard
memory limit for native parsing or capture. Fresh selected tests and OFF/ZAA
comparisons pass without changing golden inputs, normalization or test policy.

This review is by the implementation author. Independent review, source-input
ownership, model/plate/worker/revision binding, tool/scene/firmware/material
qualification and physical printing remain pending.
