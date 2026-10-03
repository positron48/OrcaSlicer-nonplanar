# B14 native analysis view: author critical review

Date: 2026-10-03. Source/commands: the separate frozen source manifest and
B14-native-gui.md. This is author review, not independent safety qualification.

1. **Configuration boundary.** Traced actual Plater full-config/filament-map
   selection and BackgroundSlicingProcess plate overrides. The extraction keeps
   the old single/multiple-extruder branch behavior. Dialog recapture uses the
   actual current selected plate. Unsupported/missing metadata refuses; the
   positive fixture does not silently substitute compatibility settings.
2. **Ownership and races.** Main-thread source capture plus owned Model/config
   isolate the worker. Background code accesses no dialog/Plater widgets. Atomic
   cancellation/stage and existing Job controller carry only bounded messages.
   Finalization recaptures live inputs, exact bytes, signed-zero origin, original
   mode and monotonic epoch; private current owner/source identity must match.
   Unknown/Cancelled terminal token invalidation precedes display. Actual
   one-bit/ABA/replacement/late callback tests refuse diagnostic adoption.
3. **Lifetime and queue.** Existing idle queue only; no unrelated cancellation or
   replacement API. Modal timer drains queue events. Close defers until cancel
   completion; wxWeakRef plus owned run state protects forced destruction.
   Actual final GUI close/cancel frees the queue. Parent apply/capture still lacks
   hard interruptibility/RSS containment and is explicitly pending.
4. **Display authenticity.** Only independently parsed public worker diagnostic,
   report and original movement rows enter the dialog. Candidate bytes and proof
   credentials do not. Serialized oracle compares all 2098 rows to a different
   native owner; actual final GUI first/last XYZ/E/F/volume/byte ranges match that
   reference. Cumulative addition and decimal rendering expand time bounds.
   LOD is preview-only, selected row exact. Full role/witness/scene UI is pending.
5. **Runtime provenance/isolation.** Executable is fixed by build, copied beside
   app and checksum-bound in the isolated bundle. Runtime network/write probes
   passed. Actual Keychain startup exposed a broker path that sockets do not
   isolate; lab now skips agents before broker entry. Normal startup without
   flag keeps its branch. Flag alone does not certify isolation. Windows/Linux
   GUI packaging runtime and hard OS containment remain unqualified.
6. **Observed failures.** Kept AutoForFlush/missing plate metadata/nonidentity
   source transform refusals. Synthetic metadata corrections are separately
   recorded; actual mesh/source audits remain. Kept failed build/GUI automation/
   SHA-encoding checker observations. Source guard, math, original negative
   tests and goldens are unchanged. Process samples distinguish AX automation
   failures from observed engine hangs; no full GUI reliability claim follows.
7. **Regression and limits.** Final 470 CTest, focused 12/9270, 15 independent
   oracles, ten actual CLI scenarios and six original OFF/ZAA comparisons pass.
   Fifty of 51 parent artifacts are exact; only software-bound report changes.
   Fixed17 still 4 PASS +13 UNKNOWN, export BLOCK. Full B/P2 and independent
   review remain pending; none of these checks imply physical print safety.
