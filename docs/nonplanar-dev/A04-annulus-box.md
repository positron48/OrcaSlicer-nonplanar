# A04 finite annulus / box continuous gap

Geometry contract version 3 supports a horizontal finite annulus against an
axis-aligned solid box. It computes the nonnegative set distance from the XY
radial-range gap and independent Z gap, retaining the empty orifice. Whole-motion
outward intervals and the existing bounded adaptive queue establish PASS;
sampled values only provide upper bounds/witnesses. The explicit UnsignedGap
metric prevents treating zero distance as a signed penetration depth. Contact
at zero required clearance stays UNKNOWN; a positive clearance can fail.

Three predefined cases first failed while the pair was unsupported (exit 42).
Tests now prove the independent 3-4-5 separation, empty-opening gap 3/8, contact
semantics, a collision with clear endpoints and midpoint, translations, reversal,
offsets, radius growth, uncertainty and work/deadline exhaustion. A fourth case
proves a diagonal route clear using subdivision and bounds its independently
known minimum 7/sqrt(2)-1/2. The old unsupported test remains, using the still
unsupported box/sphere pair as explained in ADR-0021.

macOS ARM64 Release app/native build exits 0. The selected geometry families
pass 22 cases/844 assertions with NoAssertions; CTest runs 181/181 selected
cases without skips. Six fresh OFF/ZAA comparisons pass with the existing
timestamp/additive-OFF-setting allowances. evidence/A04-annulus-box retains
commands, exit codes, red/final XML, discovery, baselines and source/binary hashes.
Author review checked the connected radial-range proof and interval composition;
independent review remains pending. Complete scene orchestration, meshes,
deposition contact and physical/export qualification remain separate work.
