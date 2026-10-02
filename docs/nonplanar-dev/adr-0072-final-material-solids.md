# ADR-0072: independent final-byte material solids and whole-region union cover

Status: accepted for synthetic declared constant-flux linear sections. Complete
B12 and guarded export remain open. Parent: ADR-0071. Normative bundle unchanged.

A final-byte prefix owns exact poses, E-derived amounts and section parameters.
The verifier now classifies a complete closed build-plate box against its actual
nominal/upper/lower solids. Projection of all points is bounded analytically;
vertex sampling and filled bead AABBs cannot provide positive evidence.

Nominal is the finite rectangle/stadium section swept along the actual XY line,
with affine top/gap and constant XY flux. Upper uses the largest declared dose
and a symmetric local along/normal XY plus Z expansion. Lower uses the smallest
declared dose and the corresponding local erosion. Original growth/loss and
numerical uncertainty are added unchanged. Exact rationals and enclosed pi/sqrt
bound whole-section inequalities. An upper outside proof checks the expanded
query against the largest-dose solid; a lower inside proof checks every allowed
translated query against the smallest-dose solid. A fixed admissible translation
can prove upper inside or lower outside. Unresolved boundaries remain UNKNOWN.

Lower finite butt erosion is retained separately on every event. Short packets
can have empty lower solids. No original packet/run certificate is reused after
byte rounding/dose uncertainty, and no joint seam is manufactured. Continuous
joined packet solids, actual support gaps and material/head contact remain
required where per-event erosion is insufficient.

Union cover partitions the requested box into closed, shared-boundary leaves,
each proved entirely inside one actual-prefix bead. A fully outside subbox is a
missing-coverage witness; an unresolved cell, split/depth/work/deadline limit or
stale callback returns UNKNOWN with no partial certificate. Positive evidence is
protected and retains source prefix, representation, query region and owners.
Overlaps do not duplicate coverage requirements and holes are never filled from
an outer bounding box. The result is geometric region coverage, not union volume
or full support/contact/export approval.

All query inputs/limits are captured before callbacks. Work starts at the prefix's
cumulative count; one deadline covers preparation and the complete partition.
Source/rate/material freshness and the numeric environment are checked again
before publication, including negative witnesses. No persisted schema, settings,
profile qualification, complete job state or export route changes.
