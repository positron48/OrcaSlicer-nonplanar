# ADR-0055: Bound lower material across continuous packet runs

Status: locally verified; independent review pending.

## Invariant

For one declared continuous nominal extrusion run N, a closed requested box K
has a run-level lower certificate only if

    K + [-xy_loss,xy_loss]^2 x [-z_loss,z_loss] is contained in N,
    xy_loss = inner_xy_loss + numerical_coordinate_error,
    z_loss = inner_z_loss + numerical_coordinate_error.

The square includes the corresponding XY radial displacement uncertainty. No
original loss is reduced. The nominal union is the actual constant-flux material
of the selected G1 packets, with their original binary amounts, XY lengths and
affine gap/Z sections. The expanded box remains strictly inside the run's real
ends, including the actual partial current front. Future packets are absent.

## Decision and model boundary

Add protected material-run contract 1. `reconstruct_material_run` owns the
original immutable prefix and an inclusive consecutive record range. Admit only
consistently directed X/Y extrusion, exact XYZ continuity and matching section
kind/material/support/contact/patch provenance. Reject travel, retract, reversal,
turns and foreign context. Width and flux may vary; no packets are merged into
one averaged bead. The existing independently eroded per-event D_lower and its
negative native lower-corner case retain their original meaning.

This is an explicit continuous-run geometric model, not a deduction that
independently displaced finite beads must have the same lower union. Applying
the loss to the boundary of the continuous run is a declared model assumption;
independent packet interruptions or unqualified physical extrusion disturbances
are outside it. Neither continuity of G1 commands nor this snapshot calibrates
the uncertainty or proves physical bonding. Separate protected result types and
reason strings prevent substituting this model for an old per-event proof.

`cover_material_run_lower` expands the entire box outward and clips its
longitudinal extent at every exact actual packet boundary. Each resulting
complete slice must be continuously inside its nominal section. Internal butt
faces are closed; real run ends remain strict. Reuse the nominal section
predicate with an explicit private closed-face flag; old callers keep their
original default. Exact rational longitudinal clipping, outward interval
sections and bounded longitudinal subdivision avoid point/vertex-only passes.
A narrow neighbour or raised floor in the inflated domain must refuse.

`find_material_run_join` reuses the bounded common-box search with run owners.
Only disjoint record ranges in the same original prefix may join. Nominal
enclosures prune/search; only whole inflated-box coverage certifies. A partial
coverage failure is UNKNOWN for that search box, not proof of disjointness.
`assess_first_cap_run_joins` reuses the original packet/path ledger mapping and
requests four corners plus both ends of each interior path. Capture, all
nominal-section checks, subdivisions and search proposals share the original
cell/work/deadline limits. It returns the complete requested list or no proof.
Callbacks remain active through publication; caller wrappers/limits are copied.

This adds no cache, serialization, IR, 3MF or profile format. Existing contracts
and persisted fingerprints retain their meaning. Rebuild all application/native
consumers; new run snapshots own their original prefix and cannot be fabricated
as aggregates. No actual cap path, amount, target, ROI, precision, original
inner/numerical loss, contact exception or public default is changed.

## Evidence and remaining dependency

The unchanged native wedge's five paths/161 packets now provide six local
run-level common lower boxes, while its original per-event proof still refuses
paths 2/3 and retains nine empty lower packet sets. A whole long-run box is
continuously covered through 53 actual packets. An independent 113-bit oracle
bounds each complete expanded slice from actual binary amounts and affine
sections; it does not call production predicates. B07-material-runs.md records
commands, counts, original negative evidence and limits.

Local witnesses do not prove allowable total excess, floor/body bonding,
volumetric seam, complete layer connectivity, remaining 3D fill, later curved or
stepped support, head contact/access, legal order/flow or export permission.
Independent safety review, Windows and operator qualification remain pending.
The full B01-B15 objective remains active and public export stays blocked.
