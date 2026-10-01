# B07 central first-cap width replan

Invariant and mathematical scope: ADR-0059. Replace central infill nominal widths
while retaining every original XYZ/gap, all four contour snapshots and every
extended end packet/dose/error. The original actual body, target, band and losses
remain owned and unchanged. Require positive whole-ledger S/R reduction with
bounded C loss/M increase and the original spill ceiling.

Two new cases / 2897 assertions pass within the final combined material suite.
Both axes use flat .45 -> .40 mm and sloped .45 -> .43 mm candidates before and
after end replan. Flat cases also extend ends after width replacement, preserving
all mixed-width central packets. Independent 113-bit midpoint dose, the analytic
integral difference k*L*(h1-h0)^2/12 and flat section rectangle sweeps verify the
actual amounts and total multiplicity. Captured wrappers and all unchanged
XYZ/gap/contour/end fields are checked. An independent finer section sweep proves
that .28 mm narrowing creates more than .001 mm3 new voids; it refuses coverage.
Absent/repeated/same/wider/too-short widths, inherited precision, excessive loss,
insufficient reduction, spill, work/cells/packets, stale/cancelled/late publication,
timeout and unsupported rounding publish no snapshot.

The unchanged native sliced body/target has five paths and 173 packets. The
moderate .40 -> .38 mm replacement changes 53 central packets and retains 120.
Whole measurements in mm3:

| Quantity | Narrow central cap |
|---|---|
| S | approximately .394386 |
| R | [.174812,.175043] |
| R reduction | [.00605963,.00652244] |
| C | [.218986,.219409] |
| M | [.0955759,.0959988] |
| Spill | [.000164691,.000356254] |
| Under-material missing | [.00866917,.00966917] |
| Vertically clear missing | [.0863297,.0869067] |

Commanded S falls by approximately .006291 mm3 from the extended candidate.
Coverage change/M increase fit the original .001 mm3 ceiling. Real positive
missing and repeated volumes remain. Six complete continuous-run joins pass
with independent whole-box checks (52 cells / 786 work), and all 173 complete
flat-floor/body interface packets pass (174 cells / 5685 work). Independent
113-bit actual-floor targets and deposited sums bracket the changed ledger.
The .30 mm candidate reduces excess but refuses the unchanged minimum flat-floor
interface width; this negative remains explicit. The original per-event corner
and short lower-packet refusals also remain.

Width replacement/whole measurement uses 515 cells / 15657 work. Recomputed
voids use 59717 cells / 847691 work and overlap the original candidate's void
intervals. The original complete policy remains 65535 cells / 2000000 work /
5 seconds with .001 mm3 precision. Finite group envelope faces now guide shadow
splits as they already guide box union; every child retains the original
continuous occupancy/roof bounds. No bound is inferred from an envelope alone.
Defaults, material losses and support thresholds are unchanged. Observed local
time is not a portable performance guarantee or full SYS-08 measurement.

The Release application and both native consumers build on macOS ARM64 with
Apple Clang 21. Combined material suites execute 99 cases / 30401 assertions;
native-body suites execute 14 cases / 112938 assertions. Selected CTest executes 344/344
without skips. Six fresh OFF/ZAA G-code/modal comparisons pass against pinned
stock with unchanged normalization and the established additive OFF default
allowance. Source inventory/package checks pass.
Build14 final source/binary hashes bind the result; retained configure label
31461d50 leaves complete B13 software provenance pending.

Historical failures remain archived: missing API, a test member-name typo,
over-narrow merged-core/budget trials, an incorrect exact-dose test assumption,
coarse independent quadrature and native .30 mm floor / .34 mm shadow-cell
and deadline refusals. A combined-core shadow trial did not materially reduce
work and is excluded from final source. The final .38 mm native width qualifies
the aligned merged-core kernel and unchanged full interface; the .34 mm trial
needed the general union solver and had a borderline shadow deadline. The dose
test now independently verifies commanded midpoint amount and
its analytic integration error rather than requiring an exact integral. The
oracle increases height samples without loosening its .0001 mm3 width. Moderate
positive widths preserve flat cores; the .28 mm true coverage-loss negative
remains. Shadow splitting retains all continuous occupancy formulas. The final
moderate
width and whole-cap kernel provide the same-budget positive native candidate
without removing previous negative cases. An independent ordinary flat shadow
sweep also
verifies the covered/under-material measures after width replacement. The wider
analytic extended flat cap exceeds its shadow deadline at the same budget; that
trial is retained and this wider classification remains unqualified.

Raw outputs: build/nonplanar-evidence/B07-first-cap-width-replan. Exact argv,
statuses/exit codes, XML and source/binary/raw/archive hashes:
evidence/B07-first-cap-width-replan/source-manifest.json. Compression preserves
original bytes after decompression. Incoming specification/goldens are unchanged.
First-cap runtime contract is 3; width replan is 1. Each owned packet's nominal
width is authoritative, with original policy retained as provenance. No persisted
schema/cache/IR/3MF, profile or public default changes.

Linux for this revision, Windows, independent safety review and physical
execution remain NOT_RUN. Parent 383d0595 Linux CI is in progress in the saved
observation; it does not verify this source. Public guarded export stays BLOCK;
full B01-B15 remains in progress. Next: qualify allowable repeated material and
complete shoulder/seam volumes, remaining 3D fill and actual later support, then
complete head/contact/order/flow and final-byte replay/job integration.
