# Small tasks following bootstrap

Status: prepared, NOT_STARTED. Normative IDs and prerequisites remain those
in `../nonplanar/backlog.json`. These tasks do not close Gate A by themselves.

| Task | Restricted change scope / deliverable | Positive and negative evidence |
|---|---|---|
| A04 (after A03) | `src/libslic3r/Nonplanar` tool primitives, bounded sweeps and corresponding native tests. Start with finite disk/plane and translating asymmetric box/wall, fixed orientation. | GEO-01: fixed asymmetric head orientation; GEO-02/03: outer-tip/full-gradient clearance including constant-Z transverse path; GEO-04: free endpoints with obstructed middle; GEO-05: duct collision with free nozzle; GEO-07/08: uncertain bound, unsupported pair or timeout → UNKNOWN. No sampled-only PASS. |
| A05 (after A03/A04) | Small native transition experiment and ADR deciding the body reservation hook. No GUI. | INT-01/02/03 and VOL-01: accepted dense support example and rejected stair gap; independent integral of reserved/body/cap volume; preserve an unsupported transition as negative fixture. Scope of valid geometry explicitly bounded. |
| A06 (after A02/A03) | Tiny ordered IR adapter to native `GCodeWriter` plus a separately implemented limited parser. No use of `_extrude` ZAA branch. | ORC-06/07, GCD-01: nonzero absolute Z across nominal layer boundaries; k=1 and k≠1 E; mutate XYZ/E, modal state, rounding and unknown command. Include plate offsets and writer's omission of unchanged Z. |
| A07 (after A03) | Versioned simulation scene/profile contract and operator measurement worksheet. Existing measurement template remains unconfirmed. | PRF-01/06, ORC-38: finite tip/full head, missing dimensions and out-of-coverage scene reject; conservative moving-part envelope with stated domain. Real dimensions/photos must come from the operator. |
| A08 (after A04–A07) | Critical review against implementation/analytical evidence, CPU/memory benchmark and revised effort estimate. | SYS-08, DOC-01: count executed tests, retain negative cases and useful positive paths; identify unresolved hooks, NOT_RUN and numerical/physical limits. No Gate A completion from preview/helper tests. |

Only one owner changes the IR/numerical contract. Any contract change needs an
ADR, consumers/migration review and corresponding test changes before parallel
work on dependent modules. Printer operation is never an agent task.

A07 first machine: **Snapmaker U1**, selected by the user. See
`A07-snapmaker-u1.md` for the unconfirmed configuration and measurement needs.
