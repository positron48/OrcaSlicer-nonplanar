# ADR-0009: bounded simulation scene contract

A07 adds an internal C++ simulation_scene_version=1 contract. IR version 1 and
immutable incoming JSON schemas/examples are unchanged. No new 3MF or persisted
profile importer is introduced. Future serialization must version and validate
these fields separately; profile_id/revision are identities, not fingerprints.

A scene declares a convex box of nozzle positions, a finite physical scene box,
a finite annular tip and positive-size fixed-axis outer boxes for nozzle body,
heater, sock, duct, sensor and mount. All six semantic parts are required. Boxes
may overlap or share geometry. Their presence is structural completeness of a
synthetic model, not evidence of a real head or proof that measurements enclose it.
The tip is centred at the nozzle opening; its outer radius strictly exceeds the
opening radius. Geometry below the tip plane is outside this initial domain.

Each moving head component must declare an outer envelope covering every
configuration over the entire nozzle domain, expressed in nozzle-local axes.
For example a gantry with a different dependence on XYZ needs the union of its
relative positions, not a box copied from one pose. Obstacles are fixed physical
outer boxes; obstacle_inventory_complete asserts the synthetic inventory covers
all configurations (moving obstacles require their physical union). No kinematic
or measurement proof of either declaration is inferred.

Coverage uses outward-rounded interval Minkowski sums of the full nozzle-domain
box with each local outer box and declared uncertainty. The square hull of the
tip annulus is checked independently. The lowest omitted upper geometry, at the
minimum nozzle Z minus uncertainty, must be strictly above the scene ceiling.
Every obstacle including uncertainty must fit the scene box. Finite straight
motions must remain in the convex nozzle domain: endpoint membership proves
segment containment here, but does not prove collision freedom.

Interval arithmetic is shared with A04/A05 under the same strict floating-point
flags and environment checks. Resource/domain limits are 64 head envelopes,
10,000 events/obstacles, and +/-10,000 mm. These are implementation limits, not
machine motion limits. Unsupported version/qualification, missing geometry,
invalid inputs, coverage failures and numerical failures remain explicit.

The sole positive status is SimulationOnly. A known colliding obstacle is an
intentional positive *coverage* test so this API cannot be mistaken for clearance.
It never returns Verified, clearance PASS or export ALLOW. All real-machine
qualification requests, including toggling a confirmation flag or changing
provenance alone, are unsupported. Production export routes still do not exist.

A07's operator worksheet is a separate unconfirmed document, not a loaded printer
preset. There is no runtime cache to invalidate in this pure function; every call
rechecks the supplied scene/events. Future snapshot/hash binding, measurement
validation, identity firmware contracts, kinematic evaluation, actual collision
queries, gauges, GUI and export policy remain subsequent work.
