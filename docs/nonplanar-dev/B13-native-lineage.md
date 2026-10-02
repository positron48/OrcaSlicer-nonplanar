# B13 — owned native body/cap/candidate lineage

The old version-1 manifest can associate an independently built candidate with a
current native job. This stage adds an executable dependency chain from that
job's original captured STL bytes, pre-apply graph, placement and slicing options
to its actual partition, native planar body, material, affine hatches, selected
first-cap assembly, linear plan and final candidate. It remains diagnostic:
source-to-plan qualification at its full normative scope is UNKNOWN/NOT_RUN.

Commit: `git log --diff-filter=A --format=%H -- docs/nonplanar-dev/B13-native-lineage.md`.
Source parent: `9a3ed1cacf7652652949b303efc1e49586645259`.
Evidence: `evidence/B13-native-lineage/source-manifest.json`; lossless raw mapping
includes failed intermediate runs. Normative documents and original goldens are
unchanged. Separate author review: `B13-native-lineage-review.md`.

## Contract and executed path

`JobNative.hpp/.cpp` adds three private immutable products:

- `analyze_guarded_native_body`: Analyzing token, exact named source resource,
  explicit millimetres, actual job plate/index, pre-apply transforms and source
  mesh correspondence. It executes the existing importer, placement, partition,
  isolated native body engine and full body material reconstruction. Derived
  effective print/region configs must equal the captured job's slicing configs.
- `plan_guarded_native_hatches`: Planning token for the identical job pointer and
  attempt, protected body parent, actual original target and reservation. The
  existing whole-footprint, support/integral/affine quota checks still run.
  Request and actual generated line alternatives/quotas enter its identity.
- `capture_guarded_native_plan`: Serializing token, exact protected hatch parent
  of the private cap assembler, complete original body and complete selected
  assembly prefix. Actual planner input must equal that entire assembled
  journal. Output can change only speed/acceleration downward; XYZ, dose, bead
  geometry, source IDs, pressure and order remain exact. Unbound route edits,
  coincident but different hatch owners and partial cap prefixes refuse.

These worker calls own mesh/request/policy/limits before callbacks and do not
read live Print/Model or files. Native construction must run in an isolated worker
or under serialized native ownership. Host `advance_guarded_job` and candidate
admission recapture the actual source/settings; direct edits and replacement
attempts revoke tokens. No worker result itself advances native state.

One root cooperative deadline covers capture, all nested work, identity encoding
and final publication. Body/hatch root limits are positive and at most 30 seconds;
plan capture is at most 1 second. Each existing nested budget remains unchanged
and is clamped to remaining root time. A mutex-protected exception latch retains
cancellation, stale revision, deadline, nonstandard/empty-message exception and
invalid floating environment refusals across nested catches and TBB callbacks.
No hard resource containment or general Model thread-safety is claimed.

The exact existing 13-key transport/timestamp/log omission registry now also
filters isolated derived configs. Geometry, annotations, material attributes,
unknown keys and source overrides remain identity inputs; raw host settings
remain exact and are never persisted by this stage. Native input revision and
attempt still distinguish incarnations, including raw credential edits.

## Manifest and consumers

`bind_guarded_candidate` accepts an optional protected native plan. Admission
requires its exact job/attempt/candidate pointers and hash. Such a binding uses
manifest version 2, scope `owned_native_body_cap_candidate_lineage_only` and
`native_lineage`, linking body/hatch/assembly/plan/byte identities without cycles.
The original 13-field version-1 manifest is unchanged. No persisted production
format is migrated; report version 1/registry 1 accepts both protected scopes.

The report independently replays final rates and material again. All 17
mandatory IDs remain; four declared components PASS/RUN and thirteen full-job
domains UNKNOWN/NOT_RUN. The bounded lineage does not mark
`source_model_plan_binding` PASS. Host completion remains Unknown/BLOCK.
No Verified, publication, cached/export route or printer action is added.

The standard-library oracle accepts both manifest versions, recomputes every
body/assembled/planned journal hash from actual canonical rows, checks ancestor,
job/attempt/original-file links, entire original body prefix and exact planner
row preservation except reduced limits. Rehashed semantic tampering refuses.
This is independent identity checking, not geometry/math/authenticity proof.
Linux CI captures and checks the new native lineage report explicitly.

## Verification

macOS ARM64, Apple Clang 21.0.0. New C++ file has no PCH and uses
`-fno-fast-math -ffp-contract=off`; `/fp:strict` is registered but Windows NOT_RUN.
The regenerated compiled Git label is the parent `9a3ed1ca`; exact final source,
dependency and binary hashes are retained, without software qualification claims.

- Final four-target Release build exit 0: `build-final.txt.json`.
- Five new JobNative cases, exercised with original job/context/artifact/report
  cases: 21 cases / 666 assertions, 0 failures/skips, `focus-final2.xml`.
- Exact selected CTest discovery/execution: 419/419, 0 failures/skips,
  97.213 seconds including the logged gate, `ctest-final2/results.xml`.
- Independent report oracle: 5 positives / 46 refusals, including rehashed
  source/unit/owner/ancestor/XYZ/volume/order/limit/body-prefix changes.
- 100 original independent CLI scenarios pass. Six fresh strict stock OFF/ZAA
  comparisons pass; only the existing timestamp/OFF-default normalization applies.
- Immutable-package and pinned-checkout audits pass. Current exact source Linux
  remains pending after push; Windows, GUI workflow and physical runs NOT_RUN.

The positive chain uses the original affine-wedge fixture and a real sliced flat
bead core. It preserves all 2034 body rows and adds its selected first-cap
assembly: 2197 total rows, 2072 depositions, 127589 final bytes; actual rate/material
report work 144333. At least one generated line has nonzero Z slope. Four-pass
prospective hatches do not imply four complete printed cap passes. The original
separate 2218-row fixture also runs, with its original candidate SHA unchanged;
its eleven legacy outputs are compared in `provenance-comparison.json`.

The first focused run failed in a test dereferencing the deliberately omitted
API-key option, after successful body generation. The test now asserts absence.
The first Python lineage replay indexed payload as acceleration; it now uses the
actual canonical speed/acceleration slots 5/6. Both failed logs are retained;
final runs and tamper cases execute the corrected sources. Author audit also
added explicit deadline and floating-environment cases before the final build.

## Remaining scope

Full B13 and B01–B15 remain in progress. Actual GUI plate membership, qualified
software/resources/import limits, complete compatibility, target/seam/curved
cap volume, complete later layers, support/contact/head/scene coverage, route
order/entry/exit/prolog/end, final filters, machine preconditions and delivered
material remain mandatory missing domains. The previously diagnosed indexed
CGAL body representation drift is not normalized or cross-platform-qualified.
Finish actual complete proofs before Verified and atomic publication/recovery/
copied-byte admission across every export channel. Standard U1 head/.4 nozzle
is retained; software work does not await firmware-version/material-brand answers.
