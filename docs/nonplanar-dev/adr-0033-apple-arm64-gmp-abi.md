# ADR-0033: pinned GMP arithmetic respects Apple ARM64 ABI

Status: implemented native fix; independent review pending.

Continuous native material coverage exposed intermittent SIGBUS in the installed
GMP 6.2.1 ARM64 assembly: rshift and mul_1. Disassembly uses x18 as a loop counter;
a crash records input limb count 4 with that counter corrupted to -343, repeatedly
writing past a stack temporary. Apple's ABI reserves x18 and prohibits its use:
https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms
The exact interrupt/context event was not captured. The observed register state
and code establish an ABI violation independently of that remaining trace detail.

Keep GMP 6.2.1 and its exact source checksum; do not update the pinned Orca base
or dependency release. Pass --disable-assembly only when the effective Apple
build target is aarch64, including Intel -> ARM cross builds. Apple Intel and
ARM -> Intel cross builds, Linux ARM and the Windows prebuilt branch retain their
existing behavior. Compiler-generated C arithmetic respects the platform ABI.

Rebuild dep_GMP and relink the experimental app/test targets. An installed/cached
old library is not fixed by editing CMake alone. The dependency cache identity
already hashes dependency sources, so the changed configure policy changes its
key. A CI argument probe exercises five actual native/cross configure scenarios
and retains the original URL/checksum. Native audit disassembles the complete new
archive and finds no x18/w18 register references. GNU make check passes 197/197.

Replacing deferred exact computations with eager rationals alone did not fix the
crash; its failed stress run is retained. After the actual dependency rebuild,
100 randomized-order native repetitions with allocator scribbling pass. No margins,
geometry negatives or UNKNOWN rules changed. Final CTest/OFF/ZAA evidence covers
both the ABI fix and the then-uncommitted continuous-coverage companion patch,
whose source hashes are explicitly retained. This is not isolated commit-only
proof of the companion implementation. No printer state or user presets changed.

Portable arithmetic can affect Apple ARM64 performance; bounded fixture timing
and mathematical regression pass, while full 200k-event SYS-08 remains pending.
Windows and new-revision Linux native execution remain unverified. This is an
implementation/build prerequisite, not physical or independent safety approval.
