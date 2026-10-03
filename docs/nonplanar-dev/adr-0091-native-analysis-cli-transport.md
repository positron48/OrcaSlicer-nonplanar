# ADR-0091: real Orca native analysis action and editing transport

Status: accepted for bounded CLI/transport/diagnostics. Full B14 and Gate B remain open.

The common backend is now invoked by the main Orca executable through
`--nptop-analyze request.json`. It follows the existing native model/settings
resolver and transform pipeline, applies that actual model and resolved config
to a fresh Print, owns original source file bytes and invokes NativeAnalysis.
An explicit data directory and a single analysis action are required before
loading inputs. Other actions cannot run before or after analysis. No stock slice,
post-processing or export follows the blocked diagnostic.

The version-1 JSON editing transport includes the full reservation's float32
vertices, winding-preserving indices and face type/float64 area properties;
body/material, complete head/scene/clearance, motion/serializer/replay, units,
affine pass/hatch/contour and fill choices. Parsing bounds bytes, depth, counts,
integer domains and registries; duplicate/unknown keys and lossy float32 vertices
refuse. The independent existing scene grammar is reused. Every decoded request
goes through the original private native request capture. Raw transport bytes
also belong to the actual job as a SourceFile role. This is not a second project
format, an imported proof credential or native 3MF extension implementation.

CLI owns a scoped lock-free SIGINT flag and restores its earlier handler.
Progress uses explicit NPTOP_PROGRESS stage records. NPTOP_DIAGNOSTIC contains
the actual public job/manifest/report identities and per-movement final-byte
positions, E/feed, timing/volume bounds and byte ranges. It contains no raw
candidate G-code or export credential. An input refusal contains no completed
report/replay. Every current analysis returns nonzero because fixed17 remains
4 PASS/RUN +13 UNKNOWN/NOT_RUN, export BLOCK. The same diagnostic formatter and
request codec are available for a later isolated native GUI worker.

The scoped path retains the current explicit selected affine ROI/first cap and
simulation-only resource policies. Complete native GUI/worker isolation,
whole-cap/later-pass planning, qualified contact/seams/order and source/software/
physical qualification, other CLI operations, 3MF and publication remain open.
Cooperative native limits and opaque file capture do not supply a hard process/
RSS limit; no interactive CGAL task is exposed by this action.

Verification and exact source/binary evidence are recorded in B14-native-cli.md
and evidence/B14-native-cli/. Array orders and the invocation boundary are
recorded in native-analysis-transport.md. Linux CI includes the editing oracle
and a real main-CLI gate in a separate network namespace; execution of this
new head remains pending. Original SPEC/DATA_CONTRACTS remain unchanged.
