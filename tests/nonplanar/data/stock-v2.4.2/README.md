# Original stock baseline — software regression only

Captured from unmodified source at
`8500fcdccaa10b5099ac20d252af3a7c560046f1`, native macOS ARM64, before adding
Nonplanar C++ types. This is the first capture; no older golden was replaced.
Six successful CLI runs: flat block, 5-degree wedge and shallow sphere, OFF/ZAA.
`manifest.json` records input, executable and output SHA-256; per-run metadata
records exact argv, working directory, exit code and elapsed time.

These files are **not qualified for printing**. `from: system` on the local
simulation machine JSON is Orca's standalone preset identity mechanism, not a
claim of vendor approval or measured geometry. Explicit compatibility binds
the process to that simulation machine; neither JSON is installed in presets.

Use `scripts/nonplanar/compare_baselines.py` for differential regression.
Only the exact generated timestamp line is normalized. All other comments,
warnings, configuration and G-code remain significant. The limited independent
modal replay handles G90/G21/M83/G0/G1 and compares opaque control commands
byte-for-byte. Initial XYZ can be unknown until established by the stock file.
It is not the A06/B12 firmware/material/safety verifier.

The saved sandbox policy and argv contain the original absolute workspace
paths as provenance. Re-run with `capture_baselines.py` to create a new isolated
runtime for the current checkout; do not execute the archived policy directly.
