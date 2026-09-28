# Gate A analytical sphere follow-up

Adds geometry contract v2 finite-annulus/solid-sphere queries with continuous
outward interval bounds. ADR-0010 records the formula, capability matrix and
internal variant migration. Three new native cases exercise independent hole,
rim, 3-4-5 and axial distance values, translated input, free endpoints with an
interior collision, narrow collision, uncertainty and unsupported pairs.

macOS ARM64, Apple Clang 21, Release. Build exit 0; initial unsupported-pair
run exit 42; implemented native run exit 0 (28 top-level cases, 2875 assertions);
selected CTest exit 0 (83 discovered/executed, no skips). Exact commands and
JUnit/source hashes are in evidence/sphere. Benchmark selection updated from
19 to 22 geometry/contract cases; earlier benchmark data is unchanged.

No stock slicing/writer source changed; CLI OFF/ZAA recapture, Linux, GUI,
physical measurement/printing and independent reviewer approval are NOT_RUN.
This supports one additional primitive pair, not full-head contact/material
replay. Prepared coupon and independent review remain separate follow-ups.
