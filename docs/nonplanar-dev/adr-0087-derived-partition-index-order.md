# ADR-0087: stable indices in the actual derived partition

Status: accepted bounded implementation; local verification passed.
Full B01–B15 and independent safety review remain open.
Scope: exact CGAL body/cap conversion only. Original/reservation/source bytes,
physical/contact assumptions, margins and export gate remain unchanged.

Repeated native runs have produced identical oriented geometry with body
vertices16/17 exchanged and all dependent source hashes changed. Existing
evidence retains that difference. Full provenance needs a stable actual derived
representation; interpreting those different hashes as equal would hide a real
owner mismatch and cannot qualify publication.

Build each derived native vertex array in lexicographic exact XYZ order before
the existing shared float conversion. Remap every face through those actual
indices. Cyclically rotate its three indices to the smallest first index,
preserving winding, then lexicographically order the oriented faces. Preserve
every point/triangle, exact shared interface, coordinate conversion/error bound
and body/cap volume. No retriangulation, hash normalization, proof reuse or
canonicalization of user source files/settings. Old snapshots retain their old
hashes and must be freshly reconstructed after any change.

Sorting participates in the original cancellation/staleness/fenv/deadline guard.
Existing input/output size limits remain. No production project/profile format
migration or widened contact exception. Different actual triangulations remain
different representations; this does not claim universal CGAL or cross-platform
geometry/float determinism.

Regression exercises same oriented input triangles under changed descriptor
identities and cyclic starts; it checks actual body/cap indexed arrays, volumes,
shared interface and unchanged source/reservation. Existing independent analytic
volume/interior/interface and topology/error/late-callback tests remain required.
Native whole-job lineage and final-byte proofs must be rebuilt from the resulting
owners and checked independently; previous candidates/reports are never blessed
by a remapping comparison. OFF/ZAA and the fixed17 blocking registry remain.

Local evidence: partition6 cases/6319 assertions and final-byte30/2433417;
selected CTest450/450 with no failures/skips;437 expected-status CLI cases;
six fresh strict OFF/ZAA pairs. Separate native processes give byte-identical
33 component output files including every material source fingerprint. All
oriented coordinate triangles of body22/40 and cap14/24 are preserved; original
and reservation8/12 each remain byte-exact. Of51 parent native files,39 remain
exact;12 carry actual new derived indices/dependent hashes. Original candidate
and every non-material component input/proof/refusal remain exact. No old hash
is normalized and no old owner or certificate is reused.
