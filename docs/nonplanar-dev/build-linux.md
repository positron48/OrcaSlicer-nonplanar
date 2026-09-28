# Linux x86_64 bootstrap and later CI evidence

No Linux x86_64 build or native test run has been performed in this bootstrap.
The current host is macOS ARM64. No CI remote or runner has been configured for
this new checkout. Emulation or a successful macOS run is not native Linux
evidence.

The pinned upstream entry points are `build_linux.sh`,
`.github/workflows/build_deps.yml`, `.github/workflows/build_orca.yml` and
`.github/actions/apt-install-deps/`. Inspect their exact arguments and runner
packages before setting up the native worker. Build the same locked revision
and dependencies, enable BUILD_TESTS and use nonempty CTest discovery plus
execution evidence. Record compiler/SDK, dependency hashes and test names.

This file is an explicit status record, not a tested Linux build recipe.
This was the original bootstrap NOT_RUN checkpoint; Gate C requires native execution evidence.

## 2026-09-28 continuation

The Linux CI job is now configured; see linux-ci.md and the current status.json.
Public run 36402596420 at 6ffcd91f completed dependency and native app/test builds,
then failed the selected-test step (exit 2). Detailed logs required unavailable
authentication, so the failing case remains unknown. Run 36413130735 at 1a2b6c40
has completed dependencies and is building app/tests at this observation. These
are earlier commits: current source changes have no Linux test pass yet. Future
runs emit bounded failure details in public check annotations and retain artifacts.
The original bootstrap record above is historical, not current CI configuration.
