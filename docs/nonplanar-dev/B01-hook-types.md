# B01 custom-code diagnostics reject wrong native types before serialization

Status: implemented and native tests pass; independent review pending.

Invariant: every registered custom-code field must reject a wrong native type
before invoking its serializer. Native enum serialization may index a name table
with an unchecked value; diagnostic formatting is not a safe validation step.
Both sparse input checks and the owned resolved preflight must report a conflict
without serializing such an option. OFF retains its early return.

Predefined regression uses a native option probe with enum type whose serialize
method counts and throws, avoiding deliberately triggering undefined behavior.
All 17 registered code hooks must reject it on source and resolved paths with
zero serializer calls. Correct empty native text/list fields remain accepted.

The red run recorded 34 serializer exceptions and 17 failed zero-call checks
(exit 42). After validation-before-formatting on both source/resolved paths,
all 37 B01 cases/3559 assertions pass with NoAssertions. macOS ARM64 Release
application/native build exits 0; selected CTest runs 192/192 without skips and
six fresh OFF/ZAA comparisons pass with the established timestamp/additive OFF
setting allowances. Evidence includes exact commands/exit codes, red/final XML,
discovery, baselines and source/binary hashes. No serializer fallback, new
compatibility allowance or guarded-export route is introduced.
