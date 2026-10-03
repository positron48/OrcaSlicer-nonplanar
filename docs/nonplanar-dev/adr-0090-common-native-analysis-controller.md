# ADR-0090: common owned native analysis controller

Status: accepted for the B14 execution backend; native CLI/UI integration and
full Gate B remain open.

The production job/body/hatch/candidate/replay factories existed, but complete
orchestration was duplicated in test fixtures. NativeAnalysis now owns an explicit
editing request and executes capture, native body, affine hatches, first cap,
material reconstruction, original scene/linear motion, serialization, protected
lineage, independent replay and blocked-report admission in one controller.
The same controller is intended for native CLI and an isolated GUI worker.

The request snapshot has a private value constructor. Capture bounds the mesh
and scene before copying and encodes exact binary values of every body/scene/
clearance/motion/serializer/replay role, reservation, units, footprint/patch,
pass/hatch/contour choices and fill region. Its canonical bytes are an additional
SourceFile role named native-analysis-request-v1 in the actual job. All worker
inputs come from that owned snapshot. No earlier selection-stage proof is reused.
The original B13 owner/attempt/phase and typed-resource comparisons remain.

One cooperative deadline (maximum 30 seconds) and exception latch cover the
entire call, including original job capture, progress callbacks and nested work.
Capture/binding/admission/replay retain their original one-second ceilings. Each
producer's separate work/geometry limits also remain; this is not a new common
cross-domain evaluation budget or a hard process/RSS limit. Progress callbacks
are bounded by the same deadline and cannot revive an old task.

The binder advances its own Serializing token. During that host operation the
root guard checks deadline/cancellation; the binder's original before/after token
checks remain authoritative. After return the controller adopts the actual
Verifying token. Admission deliberately invalidates it as Unknown/Failed. Final
root checks use no callback or working-token assertion after admission. No code
calls a generic token guard on a valid internally completed transition.

The returned snapshot owns protected native lineage and actual final-byte
rate/material replay, including per-record maps and rates. It is a blocked
internal diagnostic, not a published file. Any refused stage returns no completed
snapshot or partial candidate. A newer attempt survives an older failure.

No GUI or CLI entry point is enabled by this change. Interactive native/CGAL
work still requires process isolation; this serialized controller does not
supply it. Whole source/target coverage, full cap fill, contact/seams/order,
software/machine/material qualification and final publication are incomplete.
Fixed17 remains unchanged with 4 PASS/RUN +13 UNKNOWN/NOT_RUN and export BLOCK.
Next: native request/diagnostic transport, CLI invocation and isolated GUI worker.
Exact observed tests and source/binary evidence are in B14-native-controller.md.
