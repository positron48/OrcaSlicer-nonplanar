# B02: bounded source-file capture and native diagnostic command

Invariant: analysis consumes the immutable bytes captured from one opened source,
not a later read of the pathname. Input paths and options are copied before
callbacks. A nonzero revision and explicit millimetres are required. An accepted
capture owns SHA-256 and exact bytes; it never writes the source or user presets.

On macOS/Linux, the file is opened read-only, close-on-exec, nonblocking and without
following a final symlink. Descriptor metadata must identify a regular file, with
size at most 2 MiB. FIFO/device/directory inputs are rejected before reading.
Windows uses a Unicode handle, rejects directories/reparse points and shares only
read access, excluding ordinary concurrent writers/deletion. The final component
is checked; this is not a restriction against symlinked ancestor directories.

Capture uses bounded chunks, compares the final byte count, then checks descriptor
and current-path identity, size and modification/change timestamps. POSIX inode
replacement and observed same-size writes fail closed. Windows file-sharing blocks
ordinary concurrent writes; both paths retain only accepted immutable bytes.
Callbacks are checked before/through reading and after hashing. Cancellation,
stale revision, timeout or exceptions discard partial data. Later changes to the
pathname cannot change a completed snapshot or its worker analysis.

This is not an atomic filesystem-version guarantee. Metadata observations cannot
establish atomicity against every filesystem or external writer. The stored bytes
are the authority and undergo topology/geometry checks independently. File I/O is
a background operation with cooperative deadline checks; a blocked kernel/network
filesystem read still requires outer process supervision. No hard I/O deadline or
hard host-memory containment is claimed. Windows execution remains NOT_RUN here.

## Actual diagnostic entry point

The native `nonplanar_stl_audit` executable captures the file and calls the native
supervised worker, using Orca's actual-executable lookup for its trusted sibling.
Neither PATH nor a source/profile/command-line field selects the worker. It reads
no user configuration and outputs a schema-1 `stl_geometry_audit` JSON report.
One CLI process has one immutable transaction, revision 1. `--millimeters` is an
explicit declaration, not automatic STL-unit inference. For example, after build:

```sh
build/arm64/src/nonplanar_import/Release/nonplanar_stl_audit --millimeters docs/nonplanar/fixtures/models/flat_block.stl
```

Exit 0 means `VALID_GEOMETRY` in the bounded native-import domain; exit 2 means
INVALID/UNKNOWN and 64 is usage/missing explicit units. `export_allowed` is always
false. The report provides source hash, native face count, volume interval, source
conversion-error bound and observed worker peak RSS including the 64 KiB allowance.
It does not qualify transforms, solid-occupancy error, a head/material, a complete
job, toolpaths or printability. Helpers are not yet installed into a GUI bundle.

## Observed validation

macOS ARM64 Release builds with exit 0. Five native file-capture cases pass 53
assertions under NoAssertions, including Unicode paths, missing/special/oversized
files, writes/replacement during capture, cancellation, stale revisions, frozen
timeout options and analysis of saved bytes after the original path is replaced.
Selected CTest executes 119/119 with no skips/failures. Ten actual CLI invocations
pass expected outcomes: three positive geometries and seven refusals, including
malformed/open/oversized/missing/directory input and missing explicit units.

The initial CLI check incorrectly expected the normative shallow-sphere fixture
to fit the current domain. Its actual count is 6720 faces, above the unchanged
5000-face limit; UNKNOWN was correct. That failure is retained and this fixture
is now an explicit negative resource case. A separate generated sphere uses a
20x20 grid (1760 faces) for positive curved geometry; its catalog, generator
command, STL and manifest are retained. No normative fixture/golden was changed,
and this coarse mesh is not a certificate of approximation to the analytic sphere.

An early focused invocation ran before linking completed and found zero matching
tests (exit 2). It is retained as an invalid stale-binary attempt, not native-test
evidence. The completed-build focused run and subsequent full selected CTest are
the actual successful checks. Exact commands/exits, XML, CLI replies, failure
records and source/binary hashes are in evidence/B02-file. The normative bundle
audit passes. No new application/OFF/ZAA run was needed: stock import/slicing and
writer paths are unchanged; six B02-snapshot comparisons remain the latest.
Remote Linux results, independent review, GUI integration and physical evidence
are not inferred from these macOS results.
