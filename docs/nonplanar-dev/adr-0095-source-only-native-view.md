# ADR-0095: source-only native analysis view before child Print.apply

Date: 2026-10-03. Status: accepted for the bounded native view path.

## Problem and invariant

The GUI analysis run previously cloned its captured Model/config into a second
private Model and applied a private Print before launching the supervised child.
That Print.apply could not be interrupted by the child's watchdog. The existing
GuardedJobTask also required a host Print before transport could be constructed.

The view host now captures an immutable bounded source and a privately owned,
revocable display attempt. It constructs no analysis Print and performs no
Print.apply. The supervised child reconstructs the published source exactly,
applies its actual Print, resolves global/region settings and runs the existing
native controller. A view attempt can display a diagnostic once; it cannot
create a native proof snapshot or authorize any export.

## Decision

Retain internal worker protocol 1 for Print-bound clients. Add internal protocol
2 with host_view=[dialog_revision, attempt, exact_view_identity_sha256]. This is
an ephemeral process protocol, not a change to mathematical IR, request JSON,
report, project or profile formats. Both versions share the same mesh/config/
annotation encoder/decoder, source recapture and diagnostic checks. No adapter
fakes a host job or revises the fixed seventeen mandatory checks.

NativeAnalysisViewInput owns the original captured source and plate origin.
Original mode, full private source bits, exact editor bytes and signed-zero
origin remain view identity inputs. Only the existing fixed Job v1 omission
registry removes auth/network/timestamp/log settings from published transport.
The source worker factory requires the exact native-analysis-json-v1 editing
resource, actual observed source files, compiled inventory identity and a
current private view token; it refuses software/typed-request overrides.

NativeAnalysisViewOwner serializes begin/invalidate/finish on the GUI thread.
Only its atomic token validity is inspected by the background worker and monitor.
Replacement invalidates the old attempt before validating the new one. Final
adoption requires the exact current owner pointer, revision and recaptured live
identity, then revokes the token. Late or foreign completion cannot revoke a
replacement. Edits/cancel/destruction revoke it; no live widget/Model/Print enters
the monitor. Existing Plater queue, idle checks, weak callback and modal timer
remain in use. There is no second GUI worker framework.

The child emits Capture before Print.apply for version 2, then preserves the
original ten-stage sequence. Existing input policy checks precede Print.apply;
actual resolved global and region policy checks follow it. Refusal diagnostics
name the offending key without exposing hook bodies or partial replay. Source,
request, compiled inventory, host ticket and payload hashes bind the response.
The same watchdog, OS CPU/file caps, sampled RSS and strict output checks apply.

## Verification and remaining limits

B14-source-worker.md and its frozen evidence record actual source-only positive
execution, separate native reference, owner replacement/cancellation, original
policy/units refusals and malformed snapshots. The editing-resource regression
is retained red and green. The new independent Python oracle reuses source bit
framing and final-byte parsing, without manufacturing a schema-1 ticket. Its
owner check is against separately recorded display metadata; it does not qualify
private omitted settings or mint an ownership/export proof.

Source capture/config copy, bounded source-file observation and JSON construction/
parsing still happen in the parent and are not hard preemptible or host-RSS
contained. The child watchdog samples RSS; it does not establish instantaneous
macOS hard memory containment. Descendants/crash lifecycle, broad source/import/
3MF, full domain visualization/editing, cap/later passes, qualified contact/seams,
whole-job/order/geometry and all seventeen proof domains remain open. Complete
software identity remains UNKNOWN despite actual inventory hashes and recorded
cached build controls. Independent review, other-platform GUI/runtime and physical
qualification remain separate requirements. Full B01-B15 remains active and
export BLOCK; standard U1 head and 0.4-mm nozzle are known.
