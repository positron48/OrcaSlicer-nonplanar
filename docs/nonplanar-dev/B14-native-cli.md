# B14: real native CLI analysis, editing transport and movement diagnostics

The main Orca executable now invokes the common NativeAnalysis backend through
`--nptop-analyze request.json`, using the existing model/settings resolver and
transforms followed by a fresh Print.apply. Explicit datadir and exactly one
action are required before loading inputs. The action returns directly with a
nonzero exit; no ordinary slice/export follows its blocked report.

NativeAnalysisJson schema 1 transports all original typed inputs and reservation,
ROI/pass/hatch/contour/fill choices into the private request factory. Exact original
JSON/source resources are owned by the actual job. Duplicate/unknown keys,
nonfinite/incorrect types, byte/depth/count limits and lossy float32 coordinates
refuse. Original winding, face properties and full uint64 IDs remain. The scene
reuses the existing strict grammar. See native-analysis-transport.md for array
orders and invocation; ADR-0091 and B14-native-cli-review.md define the invariant.

Diagnostics carry the actual immutable job/report/manifest identities and every
independently parsed movement's index, XYZ, E/feed, outward duration/nominal-volume
bounds and original byte interval. Raw candidate bytes/export credentials are
absent. Input failure/cancellation publishes no completed report or partial replay.
Policy refusal names its conflicting parameter without raw code/values. SIGINT
feeds the common cooperative root through a scoped lock-free flag and restores
its previous handler. The main action uses production limits, never fixture
subdivision choices.

macOS ARM64 / Apple Clang 21.0.0 / Release evidence:

- All four targets build. Final source build7 exits 0 / 38.328 s; unchanged
  build8 exits 0 / 13.580 s with no compilation or linking. Compiled inventory
  has 18291 files / 2434374 bytes / SHA-256
  c37e63b6feaa41e48f25061770fe2c51c409fd9192ae5d64788ed51461a54da7.
- Five NativeAnalysis/NativeAnalysisJson cases / 8972 assertions pass with seed
  1648739313 and NoAssertions. Every original finer-subdivision controller case
  remains; a separate actual owner exercises production defaults. Round-trip
  tests include ID 2^53+1/uint64 max, signed zero, subnormal double, float32 bits,
  properties and malformed/oversized/deep/duplicate inputs.
- Selected CTest executes 463 / 463, failures/skips 0, elapsed 118.190 s. Existing
  geometry, material, contact, support, cancellation and exhausted-budget negatives
  remain. Exact names/discovery/JUnit are preserved.
- Real main Orca CLI gate passes 10 scenarios / 3.018 s, in fresh datadirs and
  deny-network/limited-write sandbox. Complete analysis invokes all ten stages.
  Units, unsupported qualification, duplicate/oversized JSON, OFF, custom code,
  missing datadir and mixed export actions refuse. SIGINT is sent only after
  actual body progress and returns NATIVE_ANALYSIS_CANCELLED. No G-code/export
  output is created. Expected analysis exit is 156; setup refusal is 254 on POSIX.
- Actual CLI candidate has 2098 records / SHA-256
  c330363d909d5907a52b151a7ca499d1ce72681cc0185edd6f9c312bef12686b;
  replay performs 137782 evaluations. Every diagnostic movement and the candidate
  hash match the separate native owner using those same production defaults.
  The earlier finer fixture remains 2197 records / SHA-256
  e338ae9a6da37420573361725b6ce3b27691edce4e78de1c37d86e11a14a3739.
- Thirteen final independent oracle commands pass. The new editing oracle
  reconstructs exact reservation binary framing and every six-role typed vector,
  scans final decimal movement bytes, links both actual reports and rejects
  ten transport mutations. It also passes on the separate focus output.
- Fifty of 51 parent candidate files are byte-exact; only the software-bound
  report changes. The original supported 2205-record candidate remains
  128014 bytes / SHA-256
  b8eb15681127c1a8db57ce0aa257326763f16832b82fdafd13ed091c4bd98ef8.
- All six fresh strict OFF/ZAA comparisons pass the original comparator with
  its existing timestamp/OFF-default allowance. Normative bundle and goldens
  are unchanged. Source/package/workflow YAML checks pass. Linux CI includes
  the editing oracle and real CLI after the application build, with networking
  disabled by unshare; its new-head execution is pending after push.

Intermediate build/test/CLI preparation failures are retained. A full editing
snapshot is not a filament preset: actual preset registries, enum serialization
maps and explicit empty hooks/plate-only controls are required. The original
custom-limit controller and production-default CLI correctly produce different
packet subdivisions. Verification compares each against its actual policy,
without changing margins, removing negatives or promoting UNKNOWN.

Exact commands/exit codes, compiler, source/dependency/binary hashes, fresh
JUnit/job/candidate records and lossless raw baselines are frozen under
`evidence/B14-native-cli/`. Commit resolves with
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B14-native-cli.md`.

The selected affine ROI/first cap remains bounded and simulation-only. Fixed17
stays 4 PASS/RUN + 13 UNKNOWN/NOT_RUN, overall UNKNOWN and export BLOCK. Full
B01–B15 remains IN_PROGRESS. Minimal native GUI/isolated worker, hard process/RSS
limits, full source capture/import diagnostics, native 3MF and full CLI operation
matrix remain pending. Complete cap/later passes, qualified contact/seams/whole
job order/geometry, software/physical qualification and publication remain open.
Linux new-head outcome, Windows/full GUI/physical tests and independent review
are NOT_RUN/pending. Standard U1 head and 0.4 mm nozzle are known; software work
continues independently of firmware/material-brand information.
