# A08 revised planning estimate

This estimates **remaining active engineering/review/operator hours**, not elapsed
agent time, subscription usage or a delivery promise. Historical active hours
were not measured, so completed build wall time is not subtracted from a budget.
SPEC's original total P2 estimate was 300–600 hours and limited MVP 120–220 hours.
The audit shows that the current work is bounded P0 foundations, not most of P1.

| Remaining block | Active hours | Reason for uncertainty |
|---|---:|---|
| Gate A follow-ups and independent review | 16–32 | Analytical sphere evidence, prepared coupon/protocol, review findings and platform evidence |
| Compatibility, immutable snapshots, measured profiles/import policy | 24–48 | Resolved overrides, firmware/domain assumptions, provenance and invalidation |
| Cap partition, actual support, transitions, paths and ordering | 60–120 | Multi-bead seams, holes, coverage and nontrivial geometries not yet solved |
| Material collision, motion limits and integrated independent verifier | 64–128 | Whole-head/current-bead contact, calibrated volume, numerical budgets and final bytes |
| Export integration, GUI/3MF, STEP/multiple caps, builds and release | 64–128 | Shared routes/filters, stale-cache tests and cross-platform regression |
| Operator qualification, repeatability, documentation and fixes | 40–80 | Requires measured hardware/material and physical outcomes; cannot be agent-certified |
| **Remaining P2 estimate** | **268–536 (rounded 270–540)** | Broad planning range; excludes unattended print time and waiting |

An integrated restricted P1 software candidate plausibly consumes roughly
180–370 of these remaining hours, depending on how much verifier/export work
can remain within the genuinely restricted domain. This is a revised estimate,
not a claim that unit-test timings predict engineering productivity. Printing
and physical acceptance may add delays independently of software progress.

Retain the original risk allowance: transition/contact failures can roughly
double effort. Re-estimate after a real end-to-end single-cap software candidate
with mandatory checks and after first operator evidence. Do not use the 56 ms
bounded serialization measurement to reduce geometric/review/qualification work.
