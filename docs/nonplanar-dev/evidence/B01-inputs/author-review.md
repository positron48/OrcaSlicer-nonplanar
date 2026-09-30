# Author review of exact native input ownership

Reviewed the final implementation and observed native test/evidence results.
Snapshot construction deep-copies indexed meshes and facet streams, owns every
captured config through const storage and preserves generic enum dictionaries.
No const Model with mutable child pointers escapes. Native source matrices and
unit/offset metadata are retained as values. Private canonical config bytes may
contain user data and must not be emitted into public diagnostics/project files.

Mesh encoding avoids structure padding, native caches, rounding and platform
endianness; framing is tested with independent Python bytes. Capturing does not
validate topology, finite coordinates, annotation semantics or arbitrary config
compatibility. Consumers must run those checks before any accepted job.

Aggregate graph/geometry/config/annotation limits are enforced before producing
a snapshot; unsupported out-of-band object controls reject. Failed apply captures
always clear the retained source and invalidate the export token. Exact source
comparison runs before normalization, under the Print mutex for replacement.
No background callback can use this API as Verified: no verification state or
export permission was added. OFF avoids capture and retains upstream slicing.

Print settings schema 2 includes raw input identity/revision alongside actual
native effective settings. Tests observe the real approximate-diff behavior,
not an invented assumption that incoming settings replace cached values. A new
Print lifetime needs a separate job ID; plate setters and other external job
dependencies still need their own complete revision binding. Revision overflow
cannot wrap; clear, also called by destruction, does not throw on exhaustion.

This is an author review. Independent review and whole B/physical qualification
remain pending. No original spec or golden byte changes were made.
