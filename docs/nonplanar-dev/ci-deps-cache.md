# Exact Linux dependency cache

The two observed earlier Linux runs each built dependencies successfully before
failing the selected native-test gate. Rebuilding the same installed dependency
prefix costs roughly 42 minutes in the completed 1a2b6c40 run. The active
042a11ac run was still building dependencies at the retained 13:31 UTC observation.
This change does not diagnose that separate native-test failure.

The workflow now restores only an exact cache key for deps/build/OrcaSlicer_dep,
the pinned source's current install layout. It does not use the deprecated
legacy destdir path. The key hashes tracked dependency recipes/patches, CMake
modules, Linux system-install scripts, build entry points, version, upstream lock,
the key helper and workflow. Generated/untracked build files are excluded.
It also binds the runner image, architecture, compiler/linker/CMake/Ninja versions,
installed Debian package versions, relevant compiler environment and the absolute
install prefix (wx-config may embed it). No prefix restore keys are accepted.

On a miss, the original build command runs unchanged. The completed installation
is saved immediately, before app compilation/tests, so a later failing test does
not discard reusable dependencies. The artifact records whether dependencies
were freshly built or restored; restore does not count as a fresh build. App
and native tests still build and execute on every selected run. The GitHub cache
restore/save actions are pinned to caa296126883cff596d87d8935842f9db880ef25, whose
v5 tag and action input/runtime definitions were checked from the official repo.

Five local Python tests pass for dependency/recipe/environment/path changes,
new/missing tracked files, generated-output exclusion and deterministic mapping
order. Ruby YAML parsing and git diff --check pass. The retained local key probe
uses explicitly simulated environment facts; it is not Linux execution. Actual
restore/save effectiveness and timing remain NOT_RUN until a workflow uses this
change. No C++ changed, so the 145/145 native and six OFF/ZAA results from
B01-layering remain the latest rather than being counted as new runs.
