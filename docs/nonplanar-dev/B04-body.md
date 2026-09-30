# B04 private native whole-body semantic adapter

Invariant: native Print processes only the reserved body mesh, using retained
source settings; all its supported object layers/regions become owned semantic
snapshots before native lifetime ends. Original source geometry/config/mode stay
unchanged. The guarded settings audit precedes the private body's OFF switch;
public guarded slicing and export remain blocked. ADR-0029 defines schema 1.

Three new native cases/2965 assertions pass with NoAssertions. The positive case
processes all 20 layers of a flat source with an interior cap reservation. An
independent long-double slab test checks complete segments against the strict
reserved interior above the floor, including lines with endpoints outside it.
Actual native top-floor paths, surrounding roof and walls remain. Object wall
count and volume width overrides survive native resolution and appear in actual
path widths. The caller's partition handle is destroyed in a callback; the
snapshot retains source/revision and its original safe_hybrid mode.

The sparse 10% fixture produces native internal bridges outside the existing
PlanarRegion contract and rejects without a snapshot; source density stays 10%.
All existing negative unsupported-role tests remain unchanged. Further negatives
cover a custom-code policy conflict that OFF must not hide, missing partition,
cancelled/stale native status work, deadline, layer/region limits and unrepresented
skirt output. A struct/hashlib Python vector independently checks full semantic
encoding (configs/partition, entity hierarchy, roles, scaled/physical points,
lengths, volumes and errors). Its placeholder provenance is encoding data only.

The adapter uses a private native Model/Print, an identity instance and already
placed body mesh; it does not apply source transforms twice or re-center geometry.
Original plate identity remains in the partition. Native PrintInstance::shift is
decoded with NativeScale as path origin. Actual full/Print/object/region configs
are retained separately from guarded settings. One owned config per native region
is reused across layers; capture does not duplicate full settings per layer.
Empty/unsupported regions, generated supports/skirt/brim and non-bed-contact
bodies reject. Aggregate entity/point/layer/region and canonical-byte limits apply.

Apple Clang 21 macOS ARM64 Release app and both targets build with exit 0. Final
selected CTest executes 231/231 without disabled/skipped cases. Six fresh OFF/ZAA
G-code/modal comparisons match pinned stock, allowing only existing timestamps
and additive nptop_mode=off. No original specification, source fixture or golden
output was changed. Failed runs are retained: test-first native compiler reports
missing API; build-1 reports missing type include/native overload mismatch; early
tests reveal unsupported sparse bridges, endpoint-only floor observation and
default skirt outside layer snapshots. The final assertions use complete segment
clipping and explicitly helper-free positive input, with a skirt rejection case.

Commands/exit metadata, XML, CTest discovery, differential manifest and source/binary
hashes are in evidence/B04-body/. Raw output is in build/nonplanar-evidence/B04-body/.

```sh
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
build/arm64/tests/fff_print/Release/fff_print_tests.app/Contents/MacOS/fff_print_tests '[PlanarBody]' --warn NoAssertions
python3 scripts/nonplanar/planar_body_fingerprint_oracle.py
python3 scripts/nonplanar/ctest_gate.py --build-dir build/arm64 --output-dir build/nonplanar-evidence/B04-body/ctest-final --target-executable nonplanar_tests --target-executable fff_print_tests
python3 scripts/nonplanar/capture_baselines.py --label B04-body-final-baselines
python3 scripts/nonplanar/compare_baselines.py tests/nonplanar/data/stock-v2.4.2 build/nonplanar-evidence/B04-body-final-baselines --allow-nptop-off-default
```

Author review found and closed loss of helper extrusions and repeated config
allocation; ownership, exception/callback lifetime, source compatibility before
OFF, native coordinate origin and complete identity were checked against source.
This is not independent critical review. The function requires an isolated native
worker or serialized host tests because native Model/config timestamps must not
race GUI edits. Opaque native processing has cooperative checks, not hard RSS/
deadline containment. No GUI/worker caller or whole-job association is claimed.

This is unordered nominal body data. Actual D_nominal/D_upper/D_lower, dense
under-cap coverage/refusal and volumetric seam proof remain pending; sparse CAD
must not be presumed solid. Whole-body hole/stepped native cases, more supported
roles and qualified native slicing error remain open. Current native positive
domain is the flat interior reservation, not all B04 geometries. B05 and subsequent
planning/replay/export remain ahead. Independent review and new-revision Linux/
Windows are pending, physical execution is NOT_RUN; complete B01-B15 remains active.

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B04-body.md`.
Diff: `git show <resolved-implementation-revision>`.
