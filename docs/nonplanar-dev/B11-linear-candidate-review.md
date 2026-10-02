# B11 candidate: separate author critical review

This reviews actual code and evidence separately from implementation notes.
It is an author review. Independent Gate A/B review remains pending.

The protected plan is the only source of rows and scheduled steps. All material,
pressure, IDs and order belong to that immutable complete plan. The candidate
stores exact final bytes/hash, policy identity and contiguous event ranges;
header state is outside event ranges and every event includes its trailing
barrier. Neither this snapshot nor parsed moves grant clearance PASS or export.

The native formatter is reused without the general GCodeWriter travel/retract
helpers. Explicit XYZ avoids a modal-Z omission/reset, and no hidden wipe or
end motion is generated. All emitted numeric values fit the native fixed buffer
and parser domains; digits are bounded before pow10 indexing. Signed pressure
uses original amounts and a separately enforced exact debt cycle, so an
unbalanced pressure cycle cannot invent restored material. The original material
source admission already refuses unequal restore/retract amounts. Dwell has no E. A collapsed
rounded physical step is rejected before byte publication.

One global downward-rounded acceleration avoids raising a previously lowered
limit. The synthetic known initial ceiling is hashed and immutable. Klipper's
M204 changes global acceleration; claiming it as intrinsically harmless would
be wrong. The installed machine state is not established here. Lower acceleration
also changes actual triangular/trapezoid timing; the original B10 timing proof
cannot be reused. Rounded length, direction and E can change final rates and
material even if their hashes agree. Complete B12 geometry/rate/time validation
remains mandatory; independent 113-bit test formulas cover only tested examples.

Coordinate conversion uses an explicit error allocation and floating allowance.
The old native zero-allocation policy still refuses. The separate rounded source
adds uncertainty without lowering .01 mm clearance, shifting head Z, dropping
material or widening any contact exception. The full native B09 exit remains
UNKNOWN. Existing geometry/collision/resource negatives must remain unchanged.

The independent parser includes no production decision helpers. Header/modes,
complete XYZ/F, pressure state, barrier/dwell state, decimal grammar and EOF are
checked independently. Late cancellation and changed rounding are checked before
return. Input and callback limits are copied before the first callback can mutate
the caller. Parsed pressure/dwell are distinct kinds and cannot become phantom
material. Unknown macros, resets and pure-E purge refuse. It is deliberately
bounded and is not the standalone complete-job verifier required by SPEC 20.

Source and both policies are owned before callbacks; work starts with the B10
cumulative count. Byte, record, work and absolute time caps prevent unrestricted
construction. Hashing/allocation are bounded but not a hard process-resource
sandbox; complete B13 containment is still pending. Final publication checks can
invalidate the constructed private snapshot and return no partial result.

Legacy A06 behavior, default XYZ/E digits and 3MF/profile contracts are preserved.
Only the adapter translation unit gains strict arithmetic/no PCH; six fresh
OFF/ZAA comparisons must establish that the old serialized outputs still match.
No presets, cloud, printer or firmware are changed.

The initial implementation preceded the new tests in this iteration, contrary
to the repository's preferred test-first order. Do not call the retained compiler
failures a behavioral red/green cycle: build1 fails on Catch logical-expression
decomposition and build2 on mixed initializer-list integer types. Their logs are
retained, no tests run on failed builds, and the sequential final build/test
results below are the actual evidence. No safety policy was relaxed to fix them.

The first focused run also exposes an incorrectly bounded new scene fixture:
its obstacle touches the scene boundary before required inflation. Pad the new
scene's lower domain to -1 mm; retain obstacle/head geometry and the original
margin/error values. The second run exposes an incorrect expectation that an
unbalanced pressure source could reach the producer: material-source capture
already refuses it. Test that actual refusal, not a forged protected plan. Both
failed runs remain in the archive. Existing negative tests and producer safety
conditions were not weakened.

Final source checks: build6 compiles application and both native consumers;
four new cases/172 assertions and complete serialization 12/1524 pass. Full native
source 2218 records passes independent byte replay and 113-bit rate formulas;
material 122/48037 and body 14/183008 pass. Final CTest and OFF/ZAA results are
recorded in B11-linear-candidate.md and the immutable source manifest. Only final
build6 supplies current-binary evidence. Native1 is historical successful output;
Native2 is the final run with identical candidate bytes and current binary.

All 371 selected CTest cases execute without failures/skips; six fresh strict
OFF/ZAA pairs match. The comparator first receives invalid named arguments,
exits 2 and is rerun with its real positional CLI; no normalization changed.
This fifth nonzero command is preserved alongside the four build/test failures.
