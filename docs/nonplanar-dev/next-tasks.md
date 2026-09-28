# Small tasks following bootstrap

Status: A04/A05 implemented and natively tested. A06 serialization/parser
experiment is the current continuation; see `A06.md` for executed evidence.
A07/A08 remain open. Normative IDs and prerequisites remain those in
`../nonplanar/backlog.json`. These tasks do not close Gate A by themselves.

| Task | Restricted change scope / deliverable | Positive and negative evidence |
|---|---|---|
| A04 (implemented; review pending) | Native annular-tip/plane and translating asymmetric box/box queries, interval bounds and limits; see `A04.md`. | GEO-01/02/03/04/05/07/08: 12 native cases, including 300 independent slab-oracle scenes. No sampled-only PASS. |
| A05 (implemented; review pending) | Small native transition experiment and ADR deciding the body reservation hook. No GUI. | INT-01/02/03 and VOL-01: accepted dense support example and rejected stair gap; independent integral of reserved/body/cap volume; preserve an unsupported transition as negative fixture. Scope of valid geometry explicitly bounded. |
| A06 (bounded experiment; review pending) | Tiny ordered IR adapter to native `GCodeWriter` plus a separately implemented limited parser. No use of `_extrude` ZAA branch. | ORC-06/07, GCD-01: nonzero absolute Z across nominal layer boundaries; k=1 and k≠1 E; mutate XYZ/E, modal state, rounding and unknown command. Include plate offsets and writer's omission of unchanged Z. |
| A07 (after A03) | Versioned simulation scene/profile contract and operator measurement worksheet. Existing measurement template remains unconfirmed. | PRF-01/06, ORC-38: finite tip/full head, missing dimensions and out-of-coverage scene reject; conservative moving-part envelope with stated domain. Real dimensions/photos must come from the operator. |
| A08 (after A04–A07) | Critical review against implementation/analytical evidence, CPU/memory benchmark and revised effort estimate. | SYS-08, DOC-01: count executed tests, retain negative cases and useful positive paths; identify unresolved hooks, NOT_RUN and numerical/physical limits. No Gate A completion from preview/helper tests. |

Only one owner changes the IR/numerical contract. Any contract change needs an
ADR, consumers/migration review and corresponding test changes before parallel
work on dependent modules. Printer operation is never an agent task.

A07 first machine: **Snapmaker U1**, selected by the user. See
`A07-snapmaker-u1.md` for the unconfirmed configuration and measurement needs.
