# Stock software baseline inputs

Simulation-only CLI inputs for the locked stock Orca. These are not a measured
printer/material profile and do not authorize printing. No user preset is read.
OFF/ZAA process files differ only in `zaa_enabled`; full resolved settings and
all actual run arguments must be saved alongside generated artifacts.

Models are the unchanged flat block, 5-degree wedge and shallow sphere in
`docs/nonplanar/fixtures/models/`. Outputs remain in the ignored build evidence
directory and carry no independent safety verification. Accepted CLI capture and its immutable records are in ../stock-v2.4.2.
Standalone machine from=system is required by the pinned CLI compatibility
logic; it is not an operator or vendor qualification.
