# Apple ARM64 GMP ABI correction

Invariant: the pinned dependency must not use Apple's reserved x18 register.
`deps/GMP/GMP.cmake` now selects compiler-generated arithmetic for the effective
Apple ARM64 target. Version 6.2.1, original checksum, compiler flags and other
platform branches stay pinned. ADR-0033 explains the scope and tradeoff.

Native material coverage twice crashed with SIGBUS in GNU MP. First EPECK lazy
addition reached rshift; switching coverage to eager Gmpq still crashed in mul_1
at randomized stress iteration 34. Crash/disassembly show a loop counter in x18,
input limb count 4 and a corrupted negative counter writing beyond stack storage.
This violates Apple's documented ABI. The exact context-switch event is not traced;
no claim is made that changing the rational wrapper fixed it.

After rebuilding dep_GMP, the entire installed archive has zero x18/w18 references.
Its SHA-256 is e8e03ef66cb6724dd8a60ab7a83fb27e1015c77da9a791133a4c01bd7b3bb798.
GNU make check runs 197/197 programs with zero skips/failures/errors. One Python
contract test exercises five architecture scenarios and the pinned source identity;
the two Apple ARM scenarios first failed and then pass. CI runs that actual CMake
argument probe before dependency work. The exact dependency cache key includes
the changed source file, so reuse must follow the new identity.

The Release experimental app and both native targets relink successfully. With
the new archive, 100 complete native repetitions pass with randomized order,
allocator scribbling and NoAssertions. Final selected CTest executes 245/245,
without disabled/skipped cases; six fresh OFF/ZAA G-code/modal comparisons match
pinned stock. These checks include the uncommitted B05 coverage companion source
patch present in that worktree. The manifest records its hashes explicitly, rather
than claiming an isolated commit-only check. Coverage has its own later report.
No immutable bundle/golden, stock installed app or user preset was changed.

```sh
python3 scripts/nonplanar/test_gmp_build_contract.py
make -C deps/build/arm64/dep_GMP-prefix/src/dep_GMP clean
cmake --build deps/build/arm64 --target dep_GMP --parallel 8
make -C deps/build/arm64/dep_GMP-prefix/src/dep_GMP check -j8
cmake --build build/arm64 --config Release --target OrcaSlicer nonplanar_tests fff_print_tests --parallel 8
xcrun llvm-objdump --disassemble deps/build/arm64/OrcaSlicer_dep/usr/local/lib/libgmp.a
```

Commands/exit codes, argument failures, archive/disassembly hashes, GNU test count,
100-iteration log, native CTest discovery/results, differential manifests and
source/binary hashes are under `evidence/GMP-apple-arm64/`; full raw logs are under
`build/nonplanar-evidence/GMP-apple-arm64/`. Crash reports and the earlier failed
stress logs are preserved in the coverage report's evidence, linked from its
manifest. Root trace shows the ABI violation; independent critical review remains
pending. New-revision Linux and Windows native checks are NOT_VERIFIED/NOT_RUN.
Portable arithmetic performance at full job scale is unmeasured. Public guarded
export stays blocked and physical execution is NOT_RUN.

Primary ABI reference:
https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms

Implementation revision:
`git log --diff-filter=A --format=%H -- docs/nonplanar-dev/GMP-apple-arm64.md`.
Diff: `git show <resolved-implementation-revision>`.
