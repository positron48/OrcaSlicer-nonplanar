# ADR-0076: owned native job context and candidate identity binding

Status: bounded implementation; author review recorded; independent review pending.
Normative DATA_CONTRACTS §6, SPEC §§19–20/22/26–27/33–35 and B13 remain unchanged.

## Decision

The native Print owns a guarded attempt. Capture the exact pre-apply source,
current executed Model/config, resolved Print/region/plate settings and all caller
resource bytes before callbacks. Source files use their exact captured volume
names; seven singleton roles are required: toolhead, scene, material, numeric,
firmware, algorithms and software. Resource bytes are opaque provenance inputs,
not parsed profiles or qualification claims. No resolver is inferred from a name.

Only private factories construct the immutable context, phase token and candidate
binding. Attempt counters never wrap. Every new capture, source change, clear,
terminal state or phase change revokes the previous atomic token. Worker code can
observe cancellation without reading native Print/Model. Host admission also
recaptures exact source/settings/executed geometry. The caller must serialize
native operations throughout synchronous capture/admission; upstream Model edits
are not made thread safe merely by the Print state mutex.

Working phases are Editing, Analyzing, Planning, Serializing and Verifying, with
Failed/Unknown/Cancelled/Stale terminals. A phase describes work ownership. It
does not prove that analysis/planning/verification succeeded. There is no Verified
or Published phase, certificate, export permission or filesystem publication.

Version-1 job JSON hashes separate publishable identity views. Those views remove
the exact registry of eleven existing transport options plus `timestamp` and
`logfile`; Print's nested source reference is replaced by the filtered source
identity. Raw full snapshots still detect edits in memory and must not be
persisted as public job data. Credentials and logging metadata therefore do not
enter the semantic identity indirectly through an old settings hash. Exact source
revision remains in the job: an edit conservatively cancels/revisions an existing
attempt even if its filtered semantic view is unchanged. Unknown overrides,
geometry bits and caller resource/source bytes remain exact. Arbitrary opaque
files are not automatically redacted. Existing native identity formats stay intact.

The exact option registry is `printer_agent`, `print_host`, `print_host_webui`,
`printhost_apikey`, `flashforge_serial_number`, `printhost_port`, `printhost_cafile`,
`printhost_user`, `printhost_password`, `printhost_ssl_ignore_revoke`,
`printhost_authorization_type`, `timestamp`, `logfile`. It is not a wildcard that
could erase an unknown slicing dependency. New credential options require an
explicit reviewed registry update. Runtime timestamps/logs are outside job JSON.

Canonical JSON retains sorted keys, UTF-8 byte identities encoded as hex,
binary64 bit strings and exact uint64 decimal IDs/revisions. It uses no NaN or
Infinity JSON number. Job, candidate-binding and old mathematical contracts are
distinct. Their versions do not authorize persisted cache/3MF migration.

For one current Serializing attempt, bind the protected final candidate bytes,
their recomputed hash/size, initial pose, both original motion/serializer policies,
actual ledger hash and source identity/revision. Allocate before advancing; after
the final callback, recapture host state and enforce cancellation/deadline before
returning a Verifying token. Refusal clears partial result payloads and cannot
invalidate a newer or foreign owner. Capture is bounded to 256 resources, 4096
bytes/name, 32 MiB total, positive timeout at most 1000 ms. Work is cooperative,
not hard process containment.

## Scope and consequences

`job_context_candidate_byte_identity_only` is an association manifest, not a
model-to-plan equivalence proof. The analytical candidate fixture intentionally
has its own source chain; no matching native geometry claim is made. Protected
plan ownership establishes byte/policy/journal identity only. Independently
replayed whole-job geometry, mandatory-check completeness, measured resources,
actual binary/resource resolution and every export route still need qualification.

Existing guarded Print processing/export remains blocked. No user preset/config,
network, printer operation, defaults, margins, losses, numeric/work/time budgets,
original fixtures or stock outputs change. Full B13 and B01–B15 stay IN_PROGRESS.
Atomic multi-file publication and a final completion marker belong to a later
dependent invariant, after a protected complete verification report exists.
