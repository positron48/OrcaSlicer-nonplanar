from pathlib import Path
import json
r=Path('build/nonplanar-evidence/B07-roof-chord');d=Path('docs/nonplanar-dev')
p=json.loads((r/'provenance.json').read_text());oracle=json.loads((r/'roof-chord-oracle.txt').read_text())
report='''# B07 first finite roof chord

Invariant: ADR-0104. A certified affine chord improves an inadequate constant
first FiniteWidth roof approximation. One same whole body row supplies the
lower concave chord; all possible rows separately bound the upper roof, with
outward curvature bow or the original clipped constant ceiling. Original
nozzle/target/policy/outer width, gap/dose/numerical budgets, work/packet/depth/
time limits, callbacks and D_nominal/D_upper/D_lower distinctions remain.
Centerline diagnostics, raised actual-run floors and later gap-policy paths
retain their previous algorithm. No export permission changes.

Two new analytical cases/100 assertions cover four single/three-row examples
in both axes, rounded shoulders and valleys between overlapping actual roofs.
Single examples produce22 roof segments/22 packets/274 work; three-row examples
76/84/1797. True cross-width ridge uncertainty still refuses depth. Work, roof,
packet exhaustion, irreducible precision, stale/cancel/late cancel, deadline,
hostile rounding and callback exceptions publish no snapshot.

The two retained native precision cases now have126 focused assertions. The
complete1722-record actual Orca body remains, with three relevant stadium rows.
Five actual transverse first finite paths each produce172 segments/172 packets/
5476 work. Their .0001 mm3 cap error allocation is divided by five; observed
aggregate860 segments/860 packets and errors fit original4096/4096/200000
single-path limits. These are individual calls, not a shared deadline or complete
cap qualification. The inherited synthetic1 mm body setting is not a calibrated
.4 mm U1 print.

The original complete transverse cap now constructs7 paths/1242 packets, then
refuses MATERIAL_UNION_WORK_LIMIT at4564 cells/200000 work, union width .002832
mm3 against its original .001 mm3 budget. Parallel construction still refuses
FIRST_HATCH_ROOF_DEPTH_LIMIT. Those original limits and negatives remain; no
complete cap, positive native density or captured density program is claimed.

Independent exact stationary/end extrema bound every continuous highest-roof
gap without the production curvature estimate. Rational dose/width/root/pi laws
and64 complete closed slabs per packet enclose actual ideal target volumes
inside captured intervals. Analytical targets are [.026381005804,.026381255129]
and [.155978658642,.155980864962] mm3 in both axes. Five actual native targets
range from [.149284551931,.149285963751] to [.149399801701,.149401213145] mm3.
Eleven mutations reject, including a flat endpoint bridge through a valley.
Constant axis rows with complete finite butts/perpendicular paths are supported;
source-to-plan geometry, Lower support, physical bonding/contact/head/route/order
and complete job proof remain open.

macOS ARM64 / Apple Clang21 Release builds both tests, worker, rate auditor and
application. The following unchanged build performs no compile/link. Four
focused cases/226 assertions include two new analytical and two retained native
cases. Fresh CTest506/506 (253 nonplanar +253 fff;467 Nonplanar +39 other labels)
passes in187.429s without failure/skip. All24 independent commands pass,17
actual isolated CLI cases,6 fresh strict stock OFF/ZAA pairs and isolated bundle
network/write/data probes pass. Original56 candidates retain54 exact files;
one software-bound report and only the transverse cap refusal progress change.
Two chord traces are additive. Original2098-record unselected G-code retains
SHA c330363d909d5907a52b151a7ca499d1ce72681cc0185edd6f9c312bef12686b.
Private/public versions, profiles/3MF/goldens and the normative bundle remain.

Exploratory failures are preserved and excluded. The initial general-domain
attempt changed legacy Centerline packets; full506 ran505 positives and one
native fill failure at the unchanged65535-cell ceiling. FiniteWidth scoping
restores that positive (64 assertions and fresh full run). Two native focus
invocations used the analytical executable, ran zero cases/exit2 and were
corrected to fff_print_tests. A globally renamed historical rounded-rate parent
path caused a harness file-not-found; only that command was corrected/repeated,
with23 prior successes retained. The first valley mutation missed its intended
interior branch; the final localized mutation checks it. No failures erased.

Raw: build/nonplanar-evidence/B07-roof-chord. Frozen exact source/dependency/
binary/test IDs/commands and lossless raw hashes:
evidence/B07-roof-chord/source-manifest.json. Author critical review recorded;
independent safety review pending. New-head Linux, Windows, GUI and physical
execution NOT_RUN before push. Fixed17 stays4 PASS/RUN +13 UNKNOWN/NOT_RUN,
overall UNKNOWN, export BLOCK and full B01-B15 IN_PROGRESS. Next resolve the
complete actual cap union within original work/error budgets, then positive
native density/captured controller/child/final-byte ownership. Full fill/layers/
seams/contact/head/routes/order/whole-job/source/software/physical/publication
remain. Standard U1 head/.4 nozzle are known; software proceeds independently.
'''
(d/'B07-roof-chord.md').write_text(report)
paragraph='''`B07-roof-chord.md` and ADR-0104 add a certified affine first FiniteWidth
roof chord where the original constant bound cannot meet its gap/dose budget.
One whole owner supplies the lower chord; all possible owners bound the upper,
including overlap valleys. Two new analytical cases plus two retained native
cases/focus4/226, CTest506/506,24 independent commands,17 actual CLI cases and6
strict OFF/ZAA pairs pass. Four analytical examples in both axes and five actual
native prospective paths fit original limits. Exact stationary extrema and
complete rational dose integrals verify every path;11 mutations reject.
Original Centerline packets/fill positives remain after the preserved exploratory
regression was corrected by finite-domain scoping. Complete transverse cap now
constructs7 paths/1242 packets, then still refuses original union200000 work;
parallel finite-width ridge still refuses depth. Fifty-four of56 parent files
stay exact; software report and only cap refusal progress change, two additive
traces, unselected2098 SHA unchanged. Fixed17 remains4 PASS/RUN +13 UNKNOWN/
NOT_RUN, export BLOCK, full B IN_PROGRESS. Next complete cap union under original
budgets, then positive native density/captured program and full fill/layers/
seams/contact/head/routes/order/whole-job/source/software/physical/publication.
Independent safety review and new-head platform/GUI/physical proof pending.

'''
for f in ['README.md','next-tasks.md','gate-b-plan.md']:
 t=(d/f).read_text();needle='`B06-native-hatch-precision.md`';assert needle in t
 t=t.replace(needle,paragraph+needle,1);(d/f).write_text(t)
f=d/'status.json';j=json.loads(f.read_text());stage='B07-roof-chord'
j['evidence_directory']='docs/nonplanar-dev/evidence/'+stage;j['raw_directory']='build/nonplanar-evidence/'+stage
j['ci']='NEW_DELIVERY_HEAD_NOT_OBSERVED_BEFORE_PUSH';j['fork_commit']='SELF'
j['next_step']='Resolve complete actual finite cap union under original work/error/deadline budgets; then positive native density and captured controller/child/final-byte ownership. Full filled layers/seams/contact/measured head/routes/order/whole-job/source/import/3MF/domain UI/hard process resources/all17/software/physical/atomic publication and independent safety review remain. Preserve original margins, negative fixtures, UNKNOWN and export BLOCK. Full B01-B15 IN_PROGRESS; standard U1 head/.4 nozzle known, software independent.'
j['tasks']['B07']='CERTIFIED_FIRST_FINITE_ROOF_CHORD_AND_PROSPECTIVE_INFILL_FACTORY_CAPTURED_CORNER_LOCAL_LATER_REPLAY_PASS_COMPLETE_UNION_DENSITY_CONTROLLER_FULL_FILL_SEAMS_CONTACT_HEAD_ROUTE_ORDER_PENDING'
j['executed_checks'].update(final_ctest_discovered=506,final_ctest_executed=506,new_native_cases=467)
j['milestone_commits']['B07_roof_chord']='SELF: git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B07-roof-chord.md'
j['B07_roof_chord_evidence']={'status':'PASS_BOUNDED_FIRST_FINITE_ROOF_CHORD_INDIVIDUAL_PATHS_ONLY','document':'B07-roof-chord.md','adr':'adr-0104-first-finite-roof-chord.md','review':'B07-roof-chord-review.md','manifest':'evidence/B07-roof-chord/source-manifest.json','new_cases':2,'focus':{'cases':4,'assertions':226,'analytical_assertions':100,'retained_native_assertions':126},'ctest':{'tests':506,'failures':0,'skipped':0,'gate_seconds':187.429,'executables':{'nonplanar_tests':253,'fff_print_tests':253},'labels':{'Nonplanar':467,'other':39}},'identity_oracle_commands':24,'CLI_cases':17,'OFF_ZAA':'SIX_FRESH_STRICT_STOCK_PAIRS_PASS','parent_provenance':{k:p[k] for k in ['total','exact','software_bound_report_changes','actual_cap_refusal_progress_changes','private_version_only_changes','added']},'versions':'ALL_PRIVATE_PUBLIC_RECIPES_UNCHANGED','oracle':oracle,'native_paths':{'paths':5,'segments_each':172,'packets_each':172,'work_each':5476,'sum_segments_packets':860,'scope':'INDIVIDUAL_CALLS_NOT_SHARED_DEADLINE_OR_COMPLETE_CAP'},'retained_cap_refusals':['FIRST_HATCH_ROOF_DEPTH_LIMIT','MATERIAL_UNION_WORK_LIMIT paths=7 packets=1242 cells=4564 work=200000 union_width=0.002832'],'default_unselected_candidate':p['default_candidate'],'compiled_inventory':p['compiled_inventory'],'mandatory':p['mandatory'],'export':'BLOCK','full_B01_B15':'IN_PROGRESS','review_status':'AUTHOR_CRITICAL_REVIEW_INDEPENDENT_PENDING','GUI':'NOT_RUN_BACKEND_CHANGE','Linux':'NEW_HEAD_NOT_OBSERVED_BEFORE_PUSH','Windows':'NOT_RUN','physical':'NOT_RUN_STANDARD_U1_HEAD_NOZZLE0_4_KNOWN','remaining':j['next_step']}
f.write_text(json.dumps(j,indent=2)+'\n')
# Preserve the final harness failure also in the author audit.
f=d/'B07-roof-chord-review.md';t=f.read_text();t=t.replace('The initial Centerline regression', 'A rounded-rate harness renamed the historical parent folder and failed before\nreading its retained witness. Only the corrected command was repeated;23 valid\ncurrent-source oracle commands remain. The final collector checks all24 argv\nand terminal exit statuses.\n\nThe initial Centerline regression');f.write_text(t)
print('Written seven milestone documents')
