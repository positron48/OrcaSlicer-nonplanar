# B06 native hatch producer precision

Invariant: ADR-0103. The combined native planner integrates the actual body roof
at min(original parent precision, half of selected hatch volume precision),
before immutable strip allocation. Existing original resource/error/deadline
limits and the final strip/whole-hatch precision checks remain. A valid coarse
stand-alone proof still refuses a finer split; no consumer retargets its leaves.

Two new actual native cases/91 focused assertions pass. The shallow original
wedge is sliced by Orca with the inherited explicit synthetic 1 mm body setting;
this is not a qualified .4 mm U1 print. The complete1722-record body stays owned.
Three relevant actual top rows supply rounded shoulders and overlaps. Wider
parallel and transverse ROIs produce4 and5 complete strips. The original .01 mm3
coarse parent yields width .00849310 mm3 on the parallel ROI and refuses the
.001 mm3 split. The selected parent uses .0005 mm3, 363 cells/2144 evaluations
parallel, with original8191/200000 ceilings. A tighter .0003 parent is preserved.
Cell/work exhaustion in either stage, irreducible .000000001 precision,
cancellation and stale revision publish no native hatch result. All original
negative cases remain.

Independent exact rational integration brackets parallel volume at
[.67920532419,.67923507409] mm3 and transverse at
[1.05253329092,1.05257955146] mm3. Complete16402/16403 closed transverse slabs
bound every point of the maximum actual stadium roof, including shoulders and
overlaps. All parent and all9 strip intervals contain the independent enclosures.
Twelve mutations reject. The checker verifies supplied selected-surface volume;
it does not independently establish source-to-plan geometry or Lower support.
Arbitrary changing-height/diagonal/partial-butt native domains remain unsupported.

Both complete first-cap attempts still refuse under their original limits:
FIRST_HATCH_ROOF_DEPTH_LIMIT parallel and FIRST_HATCH_ROOF_SEGMENT_LIMIT
transverse. The tighter integral does not resolve roof chord/packet approximation,
select a complete cap, produce density gain or bind a density program. Those
refusals are retained as the next concrete implementation dependency.

macOS ARM64 / Apple Clang21 Release builds both tests, worker, rate auditor and
application; a following unchanged build performs no compilation/linking.
CTest504/504 (251 nonplanar +253 fff;465 Nonplanar +39 other labels) passes in
194.876s without failure/skip. All23 independent commands pass, including the
new checker added to Linux CI. Seventeen actual isolated CLI cases, six fresh
strict stock OFF/ZAA comparisons and isolated bundle network/write/data probes
pass. The complete54 parent candidate files remain except one software-bound
report;53 are exact and two new native roof witnesses are additive. Unselected
2098-record G-code retains SHA
c330363d909d5907a52b151a7ca499d1ce72681cc0185edd6f9c312bef12686b.
All original recipe/schema versions, geometry/E/journals, profiles/3MF/goldens
and the normative source bundle remain.

Exploratory failures are preserved and excluded: .4 pitch violated its unchanged
numeric inset; .39 pitch exposed the coarse immutable integral; exact stored
Z4.2 was an invalid harness assumption; complete-cap depth/segments still refuse.
The first independent checker correctly rejected an altered target but its
mutation harness missed ValueError; final qualification uses the corrected
checker and matching final build/CTest witnesses only.

Raw: build/nonplanar-evidence/B06-native-hatch-precision. Frozen exact source,
dependency, binary, test IDs, commands and lossless raw hashes:
evidence/B06-native-hatch-precision/source-manifest.json. Author critical review
is recorded separately; independent safety review remains pending. New-head
Linux, Windows, GUI runtime and physical execution are NOT_RUN before push.
Fixed17 stays4 PASS/RUN and13 UNKNOWN/NOT_RUN, overall UNKNOWN, export BLOCK;
full B01-B15 remains IN_PROGRESS. Next certified actual roof chord, positive
native density/captured program, complete fill/layers/seams/contact/head/routes/
order/whole-job/source/software/physical/atomic publication. Standard U1 head
and .4 nozzle are known; software work remains independent of brand questions.
