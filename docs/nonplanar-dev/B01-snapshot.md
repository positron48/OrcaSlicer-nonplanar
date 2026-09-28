# B01 owned resolved native settings

Invariant: guarded policy evaluation and the settings retained with its result
use an owned copy of every present native option. Later source mutation or
destruction cannot change the captured values or enum dictionaries. ADR-0013
describes ownership, caller synchronization and the remaining job boundary.

Three new native cases cover every default full-print option, immutable lookup,
source clearing, exact binary64 values, nullable NaN and signed zero, nested
point groups, unknown keys and all three generic enum forms. The initial native
run failed with three enum-dictionary assertions: upstream clone() borrowed the
source maps. Copying those maps fixes the alias. An intermediate full run caught
an overly strict rejection of native extruder_type's permitted null vector map;
the final boundary preserves that raw state and rejects only a null scalar map,
whose native serializer would dereference it. These failures are retained.

Final macOS ARM64 Release app and native targets build with exit 0. Nineteen B01
cases pass, with 2845 assertions and NoAssertions enabled. Fresh selected CTest
executes 138/138 with no disabled/skipped cases. Six final OFF/ZAA comparisons
pass against pinned stock; only the existing timestamp normalization and the
explicit additive resolved setting nptop_mode=off are allowed. Source/binary
hashes, exact commands, exit metadata, failing/final XML and baseline manifests
are in evidence/B01-snapshot. No C++ change followed the final checks.

OFF does not capture the full configuration. Native Print object/region
preflight calls the new path, and all guarded export routes remain blocked.
This is not the whole JobSnapshot, a canonical fingerprint, an input-size or
memory limit, full compatibility coverage, a Linux pass or physical approval.
Model/plate source provenance, revisions and independent safety review remain
open. No whole-config serialization or sensitive-setting logging is added.
