# B02 prerequisite: bounded STL source metadata

Invariant: optional metadata must neither read outside its header buffer nor be
shared between independent/nested imports. This change is part of preparing the
native import path; it is not full guarded mesh validation or source qualification.

Source audit found unbounded `%[^\\n]` and `%s` conversions into fixed stack
buffers in admesh's ASCII STL reader. A terminal `MW` also allowed construction
of a pointer past the header. GUI binary-header extraction passed an 80-byte
non-NUL-terminated array to istringstream and leaked its manually allocated token
buffers. The metadata strings in admesh were file-static and were overwritten by
a nested import during a progress callback. The current GUI callback copies both
strings; it does not retain references beyond the call.

ASCII header capture now limits the read to 255 characters and ignores optional
metadata when the header is longer. Header continuations are skipped by the
facet counter, so a long solid name does not fabricate extra facets. Binary GUI
extraction passes an explicit 80-byte view. Both paths use the same bounded native
parser, with supported version 1.0, nonempty model/country fields, existing 127/15
character field limits and binary NUL padding handling. Metadata state is local
to each import; no global API or file format migration is introduced.

Tests exercise bounded binary headers without a NUL, NUL padding, missing fields,
unsupported versions, long tokens and a 4096-character solid name. Native ASCII
imports must keep the known cube's 12 facets, 20 mm dimensions and 8000 mm3 volume.
A nested import through the progress callback must retain each file's own IDs.
The extreme-header cases are executed only after fixing the unsafe reads; no
claim of an ASan run or deliberate execution of the old overflow is made.

Remaining B02 work: raw geometry validation before repair; bounded/stable input
capture; units; repair provenance; topology, component and self-intersection
checks; immutable source/model/transforms and worker resource/cancellation bounds.
See B02-import-audit.md. No physical or hybrid-printing acceptance is claimed.

## Observed validation

macOS ARM64 Release application, selected native suites and upstream libslic3r
suite executable built successfully. Three new parser cases / 175 assertions
passed with NoAssertions; selected CTest executed 94/94 with no skips/failures.
The existing upstream [stl] scenario also passed all 8 assertions, including
binary, ASCII LF/CRLF, nonstandard normals and a Unicode path. Other upstream
scenarios were not run as part of this change.

The added CR-at-width-boundary case first failed with exit 42 because a truncated
header was accepted as metadata. Requiring the actual LF/EOF after the scanset
fixed it; the ordinary CRLF case still passes. Initial builds include one stale
PCH failure during edits and one deliberate interruption while moving the helper
into admesh to avoid introducing a reverse library dependency. Their exits remain
recorded separately from the final successful builds/tests.

Six fresh OFF/ZAA CLI captures match pinned G-code and semantic XYZ/E/F/order;
the only resolved-config difference is the already documented B01 OFF default.
No goldens, numeric margin or failure scenario was removed. The normative
package checksum/link audit passes. Evidence: evidence/B02-parser; raw output:
build/nonplanar-evidence/B02-* and b02-parser-off-zaa.

Linux CI remains in progress for the earlier CI commit; subsequent branch work
is queued. No Linux pass, ASan result, independent review or physical test is
inferred from the macOS results.
