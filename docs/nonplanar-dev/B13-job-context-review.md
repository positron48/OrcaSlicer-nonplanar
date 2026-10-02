# B13 job-context critical review

Separate author audit of ADR-0076 and final source/evidence. This is not an
independent review and does not close Gate A/B or qualify printer output.

| Threat / boundary | Source and observed evidence | Limit |
|---|---|---|
| Mutable caller/native data | Full pre-apply and executed native snapshots, copied settings/resources; tests mutate wrappers/resources/limits after capture. | Native host edits must be serialized; no concurrent live-Model safety claim. |
| Cache reuse after tiny edits | Exact pre-apply revision plus current full settings/executed geometry recapture; adjacent binary64, plate and direct volume edit tests. | No full GUI event wiring or cache/export certificate yet. |
| Delayed/repeated/foreign callbacks | Private owner/token/attempt/phase identity, atomic revocation, exact host admission; retries, same fingerprint, reentrant replacement and foreign Print tests. | A caller can advance working phases without proving their work. No Verified state exists. |
| Worker lifetime | Atomic token only; real background thread observes replacement, phase/destruction revoke retained tokens. | Cooperative cancellation; no hard wall-time/CPU isolation. |
| Invalid dependency capture | All seven singleton roles and named source bytes, duplicate/missing/unknown/empty/oversize refusal before publication. | Opaque source/profile bytes are retained, not parsed or linked to derived geometry. |
| Credentials in public identity | Fixed option registry removed recursively from owned canonical settings; nested source digest replaced; two different credentials/timestamp/log values have equal views, native attempts still stale. | Original snapshots contain exact raw settings in memory; never persist them publicly. Unknown future credentials/opaque file content need explicit handling. |
| Candidate hash / initial pose / policy / journal mix-up | Protected byte chain, recomputed hashes, scope-bearing manifest; independent Python hash/initial-pose/canonical rebuild and tamper refusals. | Association alone does not establish native source-to-plan equivalence, check completeness or verified geometry. |
| Last callback cancellation/edit | Allocation precedes phase change; final callback then host recapture/token/deadline checks; cancellation and plate edit refuse without payload. | Caller must not mutate host concurrently outside its serialized operation. |
| Failure masks a newer attempt | Catch cleanup uses exact current token/attempt; wrong owner or stale token cannot clear a newer job. | Freshness check cost is bounded by existing capture limits, not a hard execution sandbox. |
| Shared-path regression | Selected native CTest, original complete native candidate/query byte comparison, standalone CLI and six stock OFF/ZAA comparisons. | Linux requires the exact new commit's CI result; Windows, full GUI/export and physical runs remain NOT_RUN. |
| Cross-execution provenance | CTest-emitted material policy has a different source fingerprint alone; strict byte comparison refuses. Final root-directory rerun matches all eleven original files. | Origin of this source identity difference remains unqualified; geometry equality cannot substitute for provenance equality. |

Remaining: explicit compatibility registry; original-file/import/model/GUI plate
qualification; semantic tool/scene/material/firmware/numeric/software resolution;
complete independent final replay/report and mandatory checks; Verified state;
atomic publication/recovery/copied-byte checks shared by file/CLI/GUI/cache/3MF/
removable routes; full B12 geometry/contact/routes and B14/B15. Do not infer any
of these from immutable ownership or a matching hash. All guarded exports remain
BLOCK, and synthetic profiles remain unconfirmed.
