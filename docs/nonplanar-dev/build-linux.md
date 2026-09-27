# Linux x86_64 bootstrap — NOT_RUN

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
Gate A must retain this NOT_RUN; Gate C requires a real native run.
