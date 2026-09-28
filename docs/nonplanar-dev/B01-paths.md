# B01 native path-generation settings

ADR-0026 adds eight typed discrete rules: by-layer order, four rectilinear
patterns, no infill combination or thin-wall detection, and disabled solid gap
fill. The registry now checks 24 native key/type/value contracts before and
after configuration resolution. Tests construct an explicit eligible profile;
upstream defaults and user settings are unchanged.

Two new cases initially expose 18 failing checks (exit 42), then pass with the
rules implemented. They exercise all eight incompatible settings, source/value
preservation, OFF routing, native volume override cache invalidation and a layer
range override. Existing missing/wrong-type and misleading enum-label tests also
cover the new entries. All 41 B01 cases/4419 assertions pass with NoAssertions.

The first build failed because the test accessed the ModelConfig wrapper instead
of its ConfigBase view. An accidental subsequent invocation of the old executable
found no matching test (exit 2) and is not validation. Both records are retained;
the successful rebuilt red and final runs are explicitly named in the evidence.

macOS ARM64 Release application/native build exits 0. Selected CTest executes
199/199 without skips; six fresh OFF/ZAA comparisons pass with the established
timestamp and additive OFF-setting allowances. Evidence/B01-paths includes all
commands/exit codes, XML, discovery, baselines and source/binary hashes.

Author review traced native combine_infill, thin-wall and solid gap-fill
consumers. Classic perimeter gap fill is independent of this setting, so the
actual semantic-path adapter must still reject unsupported widths/roles. Full
compatibility, numeric limits, complete job ownership and hybrid planning/export
remain pending. Independent review, Linux tests of this revision and physical
qualification are not complete.
