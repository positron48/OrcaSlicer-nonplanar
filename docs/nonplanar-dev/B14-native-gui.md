# B14: native analysis dialog and serialized display adoption

Date: 2026-10-03. ADR-0093. Parent: `a77fb5823c745c6666243fc648503441b652813e`.
Delivery revision: `git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B14-native-gui.md`.
Branch: `feature/nonplanar-bootstrap`. This is a bounded implementation milestone;
full B01–B15, Gate A/B and P2 remain IN_PROGRESS. Export remains BLOCK.

## Implemented invariant

The existing Plater job queue executes the trusted sibling native worker on an
owned model/resolved config/version-1 request. Display adoption runs on the main
thread and requires the same exact live source, original mode, explicit analysis
mode, raw editing bytes, binary64 plate origin, dialog revision and current
private Analyzing owner. Edit/undo ABA, replacement attempts, cancellation,
private Print mutation, late/repeated callbacks and destroyed dialogs cannot
restore diagnostics or Verified. The private owner always ends Unknown or
Cancelled. No raw candidate, proof credential or export/send control reaches
the view. See the separate author review; independent review remains pending.

The native dialog provides strict request loading/editing, explicit mode,
progress/cancel/close, a public fixed-17 report, movement slider, XY/XZ/YZ path
prefix and selected original XYZ/E/F/nominal volume/byte range. Cumulative time
and its decimal representation expand bounds outward. Preview-only LOD above
10,000 rows is labelled and does not alter the selected row. Native source
policy and actual Print/region refusals identify a key without displaying
imported code bodies. Cocoa smart substitutions are disabled.

The resolver is the actual existing Plater filament-map/full-config resolver,
with the actual selected plate override/index/origin. Analysis changes only
private copies. The menu and no-agent startup are enabled by SLIC3R_NPTOP_LAB;
the flag alone is not isolation. CMake copies the build-selected worker beside
the app; macOS bundle, Linux/Windows install and Linux AppImage paths include it.
The isolated macOS launcher owns datadir/cache/temp and denies network and
writes outside its runtime. A startup sample found stock cloud-agent Keychain
access despite the socket sandbox, so lab startup skips those agents before
credential-broker entry. No Keychain approval or printer operation occurred.

## Executed evidence

Frozen evidence: `evidence/B14-native-gui/source-manifest.json`, `commands.json`
and `delivery/verify.py`. Each command has exact argv/cwd/time/exit code. Logs
and large artifacts use indexed lossless gzip; original failed runs remain.
Platform: macOS arm64, AppleClang 21.0.0.21000101, CMake 4.3.1 / Ninja
Multi-Config, Release, existing local dependencies.

| Check | Final evidence | Actual result |
|---|---|---|
| Build worker, both test binaries, verifier and main GUI | `build12.txt.json` | exit 0, 23.737 s |
| Unchanged build | `build13.txt.json` | exit 0, no compile/link, 13.650 s |
| Native controller/JSON/worker/view focus, seed 1648739313, NoAssertions | `focus3.xml`, `focus3.txt.json` | 12 cases, 9270 assertions, exit 0 |
| Exact CTest discovery/execution gate | `ctest3/` | 470/470, 0 failures/skips, gate 138.863 s |
| Fourteen parent identity/final-byte oracles plus new view oracle | `*-final-oracle.txt.json`, `view-delivery-oracle.txt.json` | all 15 exit 0 |
| Real main Orca CLI | `cli1/manifest.json`, `cli1.txt.json` | ten fresh offline action/refusal/SIGINT cases, exit 0 |
| Stock OFF and separate ZAA | `baselines-final/`, `baseline-compare-final.txt.json` | six fresh pairs PASS, original comparator allowances only |
| Parent geometry/candidate provenance | `provenance.json` | 50/51 byte-exact; only software-bound native job report changes |
| Final GUI observations | `gui-observation.json`, `gui-final-*.txt.gz/.png` | actual worker/report/replay, first/last movements, outward time, edit/cancel/close/idle queue PASS |
| Isolated bundled runtime | `bundle-delivery-manifest.json`, `bundle-delivery-isolation.json` | actual core/worker SHA match final binaries; network/write probes PASS |
| Source/package/format checks | final audit/check command logs | exit 0 |

New Catch cases `[NativeAnalysisView]` run an actual child, adopt its complete
2098-row diagnostic, invalidate the private token, reject repeated delivery,
then run 12 real-child mutation scenarios: vertex, instance Z, layer height,
object wall override, signed-zero origin, editing whitespace, selected mode,
original mode, edit/undo epoch, replacement attempt, private Print origin and
cancellation. The independent view oracle compares every original replay row
and candidate/request identity against a separate native owner.

Compiled inventory: 18,300 files, 2,435,409 bytes,
SHA256 `2bce383aa7268453e6b98b14b43cc1d91be92f23d342c0305d9a422dfc9edebd`.
Default candidate: 2098 records,
SHA256 `c330363d909d5907a52b151a7ca499d1ce72681cc0185edd6f9c312bef12686b`.
Original supported 2205-record and finer 2197-record candidates remain unchanged.
Fixed17 remains **4 PASS/RUN + 13 UNKNOWN/NOT_RUN**, overall UNKNOWN, export BLOCK.

## Actual GUI qualification and refusals

The synthetic CLI machine/process/filament and ordinary OFF unsliced 3MF export
were loaded only into disposable isolated runtimes. The initial real plate
selected Auto For Flush despite global Manual; the source policy correctly
refused `filament_map_mode`. A separately logged Manual fixture exposed absent
plate metadata for dynamic map/switcher; actual Print refused that key. Explicit
false metadata was added only to another synthetic fixture, through the stock
supported plate metadata fields. No production default fallback was added.

The original exporter records volume Z=3 in both component transform and source
matrix metadata. Import places the latter in `volume.source.transform`, outside
the currently qualified identity-source domain; worker returned
`NATIVE_JOB_SOURCE_PLACEMENT`. That negative is retained. Direct original STL
import passed the source/placement audit and completed analysis in the GUI.
The final-binary fixture separately restores explicit identity source metadata,
matching the original STL source, while retaining every mesh byte, component
Z=3, build XY=(20,20) and source offset Z=3. Existing oriented-triangle equality
and actual placement audits still execute. This does not qualify general 3MF
container provenance or faithful project round trips.

Final core SHA256 `e3a484eaecd49793830a53e9a5f5260f4bde040f47445584925e177aeee4e295`;
worker SHA256 `c20bc97b7342ba4477c0ccd62bdfa0ede454b91438d4cd11d5df014c999e3295`.
The native report exposes that candidate hash, 2098 rows and observed peak RSS
in `gui-observation.json`. Last actual movement:
`[20.654035,19.005581,4.450867] → [20.747468,19.005581,4.456707]`,
bytes `[121309,121373]`, cumulative time
`[1380.8025732348785,1380.8025732354706]` s. Its original readout and first row
match the separate final-byte reference; decimal bounds enclose an independent
exact sum of duration endpoints. Editing clears report/replay; cancel returns
VIEW_CANCELLED; close during work returns to Plater with the menu enabled.

GUI automation sometimes lost AX/window access after the stock import picker;
read-only process samples show ordinary idle event loops rather than an observed
application deadlock. Repeated failed observations are retained. A final clean
launch with the logged identity-source fixture completed the same view lifecycle
without that picker. Native request loading was also observed before the final
time-format-only correction; full platform/picker qualification remains open.

## Remaining scope

Parent source/config/file capture and private Print::apply are cooperatively
bounded, not hard interruptible or hard host-RSS contained. Child RSS/deadline
supervision is not hard macOS containment. Referenced volume material IDs still
refuse within the pinned Print domain. General source import diagnostics and
3MF-container lineage, full surface/body/cap/transition/head/material/witness/
cross-section displays, complete roles, time seeking and domain editing remain.
Complete cap/later passes, qualified contact/seams/full fill, whole-job order/
geometry/all17 proofs, software/material/profile/physical qualification and
publication remain open. No real printer validation or execution occurred.
Standard U1 head and 0.4 nozzle are known; software proceeds independently.

Linux new-head CI is configured and must be observed separately after push.
Windows GUI/runtime, Linux GUI/AppImage execution, full P2 GUI and physical
tests are NOT_RUN. Independent safety review is PENDING. No original negative
case, margin, contact exception, normative source bundle or golden was changed.
