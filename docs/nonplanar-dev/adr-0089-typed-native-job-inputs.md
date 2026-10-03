# ADR-0089: owned typed native job inputs

Status: accepted for bounded identity/provenance implementation; full Gate B open.

Opaque job resource bytes could describe different scene/material/motion inputs
from the actual protected native plan. Job fingerprint equality alone did not
resolve that discrepancy. The new optional NativeJobInputsRequest owns body
material parameters, complete declared simulation head/scene, clearance,
linear motion limits, serializer precision/acceleration and material replay
options. begin_guarded_job copies those values before callbacks and generates
all six non-software resource roles. It requires CompiledInputs and accepts only
source-file resources from the caller. Role overrides are refused.

The original owner/attempt is invalidated before any capture work. Typed array
limits match the existing scene producer (64 head parts, 10000 obstacles). All
encoded plain doubles must be finite; binary64 bits and full uint64 IDs remain
exact. Resource count, aggregate bytes, final freshness and one-second root
capture limits still include automatically generated roles and compiled inputs.
Cancellation and arbitrary callback exceptions cannot publish a partial job.
Declared diagnostic jobs retain the version-1 byte format and no typed pointer.

Body analysis compares the actual parameters with the owned body declaration.
Native plan capture and candidate binding compare both original and planned
scene/head/clearance, original motion limits and serializer policy. Protected
native lineage is required in typed candidate binding. Material reconstruction
keeps the declared model ID/growth/losses; existing protected cap producers may
add their calculated coordinate error. Exact derived journals and every owner
edge remain independently required by native lineage. No caller-supplied error
allowance can replace that protected journal. Final report replay compares all
seven replay identity/value fields with the owned declaration before execution.

New resource schema 1 names identify each bounded role. The outer job schema
remains 1 because its existing kind/name/hash/length rows already bind these
bytes. Existing candidate/report schema and fixed17 registry remain unchanged.
A caller can reproduce the same bytes in Declared mode without acquiring the
protected typed pointer; content identity is neither provenance nor permission.

This does not bind every future algorithm choice. Reservation, derived ROI,
pass/hatch policies, seams/contact/order and subsequent assembly choices retain
their existing owned lineage but are not all fixed at initial capture. Complete
source-to-plan/software/physical qualification, final filters, GUI/CLI control
and atomic publication remain outstanding. No profile/project format migration,
new Verified state, mandatory promotion, contact exception or export is added.

Validation: targeted positive native body/cap/departure/byte replay plus one-bit,
same-revision, callback mutation, override, nonfinite, array-bound, cancellation,
reentrant/late and omitted-lineage negatives; independent fixture vectors and
report identity oracle; selected native suites and strict OFF/ZAA comparisons.
Exact observed counts and commands are in B13-native-inputs.md and its evidence.
