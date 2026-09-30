# Next implementation tasks

Current source checkpoint is `status.json`; milestone reports below distinguish
bounded implemented primitives from the complete pipeline. Sphere evidence and
digital coupon preparation are recorded in `sphere.md` and `coupons/README.md`.
They do not qualify the physical printer. Gate A remains open.

Next software tasks, in dependency order:

1. Complete B01 compatibility and whole-job provenance, then B02 original STL
   and actual GUI/native plate binding. `B01-inputs.md` now retains exact source
   meshes/configs/transforms before normalization and invalidates the guarded
   Print revision; `B01-print-snapshot.md` retains full/effective/region settings.
   Source and actual native effective values remain distinct. Bind all remaining
   job IDs, plate changes, file/import error, software and qualified tool/scene
   dependencies before accepting worker/planner/verifier callbacks.
2. Verify new milestones on Linux. The prior A02 correction at d57e9325 has a
   successful native Linux run 36454696019; its earlier range-failure evidence
   remains historical and does not establish an exact exception call stack.
3. Extend B03 nominal affine bounds to qualified error budgets, curved surfaces,
   ROI and tool access. Existing masks, whole-footprint checks and isolated CLI
   results remain diagnostic geometry, without export permission.
4. Complete B04 body/cap partition, dense coverage and the adapter for the whole
   native body. The owned snapshot currently represents only one LayerRegion.
5. Continue the dependent planner, safety/replay and guarded export integration
   against the normative backlog; do not close P2 from these partial modules.

Independent review of A03–A08 remains required for Gate A closure. Operator
measurements, machine/material confirmation and physical coupon runs remain
separate pending work; do not substitute synthetic evidence for them.

A07 operator evidence remains NOT_RUN. SYS-08 full-cycle performance is unmeasured.
Normative dependencies remain in `../nonplanar/backlog.json`; no large UI is next.

| Task | Restricted change scope / deliverable | Positive and negative evidence |
|---|---|---|
| A04 (implemented; review pending) | Native annular-tip/plane and translating asymmetric box/box queries, interval bounds and limits; see `A04.md`. | GEO-01/02/03/04/05/07/08: 12 native cases, including 300 independent slab-oracle scenes. No sampled-only PASS. |
| A05 (implemented; review pending) | Small native transition experiment and ADR deciding the body reservation hook. No GUI. | INT-01/02/03 and VOL-01: accepted dense support example and rejected stair gap; independent integral of reserved/body/cap volume; preserve an unsupported transition as negative fixture. Scope of valid geometry explicitly bounded. |
| A06 (bounded experiment; review pending) | Tiny ordered IR adapter to native `GCodeWriter` plus a separately implemented limited parser. No use of `_extrude` ZAA branch. | ORC-06/07, GCD-01: nonzero absolute Z across nominal layer boundaries; k=1 and k≠1 E; mutate XYZ/E, modal state, rounding and unknown command. Include plate offsets and writer's omission of unchanged Z. |
| A07 (simulation implemented; operator evidence pending) | Versioned simulation scene/profile contract and operator measurement worksheet. Existing measurement template remains unconfirmed. | PRF-01/06, ORC-38: finite tip/full head, missing dimensions and out-of-coverage scene reject; conservative moving-part envelope with stated domain. Real dimensions/photos must come from the operator. |
| A08 (audit/benchmark recorded; independent review pending) | Critical review against implementation/analytical evidence, CPU/memory benchmark and revised effort estimate. | SYS-08, DOC-01: count executed tests, retain negative cases and useful positive paths; identify unresolved hooks, NOT_RUN and numerical/physical limits. No Gate A completion from preview/helper tests. |

Only one owner changes the IR/numerical contract. Any contract change needs an
ADR, consumers/migration review and corresponding test changes before parallel
work on dependent modules. Printer operation is never an agent task.

A07 first machine: **Snapmaker U1**, selected by the user. See
`A07-snapmaker-u1.md` for the unconfirmed configuration and measurement needs.
