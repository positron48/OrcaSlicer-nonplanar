# ADR-0016: single-tool policy before native normalization

Status: implemented bounded domain; independent review pending.

Guarded input may use one physical nozzle and one filament. Native diameter
vectors must have one finite positive normal binary64 value; this does not
qualify a real diameter or its material model. Filament-to-logical mapping is
exactly [1], logical-to-physical mapping exactly [0], and the sole XY extruder
offset is exactly zero (either sign). Scalar filament assignments accept 0
(inherit) or 1. The optional source `extruder` key may disappear during native
normalization; the eight resolved region/support selectors remain required.
Nullable or textual substitutes are outside this domain.

The pinned native mapping code may rewrite automatic maps after initial
configuration resolution. Require native fmmManual and disable dynamic mapping,
filament switcher, single-extruder multi-material, manual filament change and
prime tower. Reject any material painting, including a painting that currently
uses only the first filament: its segmentation path has not been qualified.
Unused Model material labels are not equated with physical filament count.

All known policy registries apply to retained sparse model sources and the
supplied global config before Print::apply normalization. Print::apply retains
an owned rejection diagnostic. Under the existing state mutex, a change to that
diagnostic cancels work and invalidates G-code export, including when native
normalization produces an otherwise identical config. The normal publication
check consults the diagnostic before the resolved policy. OFF clears it through
apply; Print::clear clears it after invalidation. No user source is rewritten by
this policy, and no absence of a conflict grants export permission.

This boundary was required by an actual native test: changing an object source
extruder to 2 with only one filament could leave psGCodeExport marked done. The
native prime-tower normalization also removes the requested enable flag for a
single used filament. Checking only resulting PrintRegionConfig cannot diagnose
those inputs. The retained diagnostic is not a whole input snapshot or cache
fingerprint. Keys discarded by an earlier importer, malformed options passed to
upstream APIs before this boundary, complete schema coverage and immutable job
ownership remain separate work. All guarded slicing/export remains blocked.
