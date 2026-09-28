# A01 isolated coexistence attempt

On 2026-09-28, the experimental ARM64 app and the installed stock OrcaSlicer
2.3.1 executable were observed as simultaneous native processes, each launched
behind the offline write sandbox with its own repository-local datadir, tmp and
cache. Actual sandbox probes passed for both launchers. The installed app was
not modified; its executable SHA-256 matched before and after the attempt.

This is process coexistence evidence only. The stock wrapper started the
original executable, but native automation could not bind its wrapper identity.
That known test PID was terminated without opening the normal stock app route.
A second attempt used a disposable full app copy with a new bundle identity and
launcher. Its process was killed with signal 9 and codesign verification failed
with exit 1 (code object not signed). No signing, quarantine or OS protection
bypass was attempted. The relationship between the failed signature check and
the process termination is not independently established.

The experimental app rendered its home page using a copy of the earlier,
disposable test presets; it quit normally. This filesystem copy does not qualify
native preset import. No test processes remained after cleanup. The original
stock user's presets, configuration and printer connections were not used.

`evidence/A01-coexistence/observations.json` records the process identities,
paths, outcomes and original binary hash. The directory also retains launcher
sources, manifests, actual isolation results, command/exit metadata and a fork
GUI screenshot, with an artifact hash index. Source HEAD was `85078cf6`; the
binary build provenance remains in the bundle manifests and B01-transforms.

Interactive stock coexistence, native preset import and signed distribution
remain unverified. The earlier experimental GUI import/slice/preview result in
`A01-gui.md` remains valid for its recorded build. No C++ or shared slicing path
changed here, so the latest 135/135 selected native tests and six OFF/ZAA
comparisons are retained rather than reclassified as new runs.
