# B04 derived partition ordering: separate author audit

Author audit only; independent safety review remains pending. Scope is ADR-0087
and the exact conversion/regression diff, not full job or physical qualification.

The defect is in actual construction of indexed derived geometry. A normalized
hash would allow unequal owners to share an identity; that is not implemented.
The converter instead orders exact CGAL coordinates before float conversion,
remaps every actual face, preserves winding by cyclic rotation, and orders
the resulting oriented faces. It leaves source and reservation arrays intact.
Different geometry/triangulation retains different actual hashes. Shared exact
conversion remains one map for body/cap, retaining coordinate/error values and
the exact interface-face checks before rounding. Post-conversion topology,
volume and original numerical budgets are still required.

Sorting is bounded by existing vertex/face limits; every comparison polls the
original root deadline/staleness/cancel/fenv guard. Throws cannot publish a
partially ordered public partition. No global mesh type, OFF/ZAA path, production
profile/project/cache format or contact assumption changes. The behavior changes
actual derived identities, so old certificates remain invalid; every native
component/lineage/report in this stage is reconstructed with current owners.

The new property is observable invariance under descriptor identity/cyclic
triangle-start perturbations. It also checks volumes/error/interface and source
preservation. Existing independent analytic ray/volume/holes/steps and negative
topology/resource/late-guard cases remain. Separate process output equality
includes actual material fingerprints. Independent oriented-coordinate triangle
multisets document preservation, while full final-byte reference tests still
execute rather than trusting an equivalence mapping as a certificate.

Thirty final-byte cases and complete selected450 CTest tests pass; original
candidate, geometry/support proofs and refusals are exact. 434 of437 analytical
CLI outputs are exact; three timeout counters retain their actual values and
UNKNOWN/CANCELLED result. All original numeric limits/margins/negative cases
remain, including old-neighbor contour FAIL and unproved cell. Fixed17 still
contains13 missing mandatory domains and blocks export.

Cross-platform CGAL topology/float determinism and full source/resources/software/
GUI/job/publication binding require further evidence. This local representation
fix does not claim whole B completion, measured head/contact or physical safety.
