# Prepared geometry coupons — no printer job

Artifact status: GEOMETRY_PREPARED. Physical coupon fabrication and qualification:
NOT_RUN. These are two 20 x 10 mm solids: a 2 mm flat reference and a 5 degree
wedge with minimum height 2 mm. They are synthetic geometry, not U1 settings.
The STL header explicitly identifies geometry-only content; no G-code exists.

Regenerate with the unchanged normative generate_fixtures.py and this catalog.
Manifest records final binary STL SHA-256, triangle counts, bounds and volume.
The wedge exact volume is 200*(2+10*tan(5 degrees)) mm3; the flat is 400 mm3.
Mesh validation checks finite nondegenerate facets, opposite edge orientation,
each undirected edge twice and volume against these independent formulae.

Later operator protocol, only after software gates and a measured profile:

1. Record profile/firmware/material/configuration hashes and resolve every unknown
   in A07's worksheet. Neither geometry nor a normal planar baseline authorizes
   a hybrid run. Do not use generic synthetic presets on the printer.
2. Establish a normal planar reference with the operator's known qualified setup;
   retain actual settings, output hash, photos, measured XYZ dimensions and issues.
3. Only after the full guarded software pipeline exists and accepts the measured
   configuration, prepare separate OFF/ZAA/hybrid comparisons on identical geometry.
   Keep candidate/replay reports and reject any mandatory FAIL/UNKNOWN.
4. Any eventual physical run is supervised with immediate stop access. Record
   marks, interference, dimensions, seam gaps and surface quality at equal photo
   scale/light. A raised dry run does not validate deposition or original clearance.
5. Stop on contact, displaced part or scraping and preserve the failing segment.
   No firmware limit increase, compensation change or safety-margin reduction.

These files fulfil digital preparation only. SPEC's physical coupon evidence
remains NOT_RUN until the operator produces and records the actual specimen.
