# Author review of owned native placement integration

Reviewed the new API, test-first failure and final native/differential results.
The adapter copies immutable binding/source/limits before callbacks and rejects
wrong plate/count/state and geometry limits before reconstruction. The temporary
native Model carries captured geometry/source metadata and exact matrices without
recentering. It reuses the tested native centering/float-write/error chain; config
resolution is deliberately not inferred from the reconstructed geometry view.

Failures discard accepted payload, including partial snapshots returned by the
lower-level diagnostic primitive. Reconstruction time counts toward the deadline.
The final success retains original source input and byte ownership; its revision
is scoped to the caller's Print/job lifetime. Actual mutable GUI membership and
all external job dependencies still need current-revision checks at publication.

This review is by the implementation author. Independent review, whole-job gate,
complete partition/planning/verifier and physical qualification remain pending.
