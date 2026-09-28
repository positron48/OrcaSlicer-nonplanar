# B01 exact owned-config identity

ResolvedConfigSnapshot now provides bounded canonical JSON and SHA-256 for every
present native option. It retains native type, nullable flag, exact binary64 bits,
percent flags, string/key bytes, ordered vectors/nested points and generic enum
dictionaries. It never falls back to rounded option serialization. Schema 1 and
its limits are specified in ADR-0023; this identity does not establish complete
job ownership, compatibility or provenance of keys lost during import.

The native encoder matches two independently predefined Python struct/json/
hashlib vectors byte-for-byte and digest-for-digest. Three initial cases failed
against empty implementations (six failed assertions, exit 42). Tests show that
adjacent doubles with identical native serialize() text have distinct identities,
as do types, percent/nullable flags, dictionary entries, signed zeros, subnormals
and distinct NaN payloads. Invalid/nil float bits are JSON hex strings, never a
numeric validation bypass. Source destruction/mutation leaves owned hashes
unchanged. The actual full native config encodes every key, nested mutations
change identity, and unsupported representations/4 MiB output/4096-key excess
throw. Encoding is independent of floating rounding mode.

The existing OpenSSL source-snapshot SHA-256 implementation is exposed as a
shared exact-byte helper; STL ownership and its independent size limit remain.
A missing explicit cfenv test include caused one compile failure, then was fixed;
the failed build metadata is preserved. Final macOS ARM64 Release app/native
build exits 0. All 36 B01 cases/3355 assertions pass with NoAssertions, selected
CTest executes 191/191 with no skips, and all six fresh OFF/ZAA comparisons pass
under the established timestamp/additive-OFF-setting allowances. The Python
oracle also passes independently. Evidence includes commands/exit codes, red and
final XML, fixtures, discovery, baseline manifests and source/binary hashes.

Author review checked framing, byte/bit order, sorted ownership and bounded
output. Independent review remains pending. Native job/plate/worker binding,
full compatibility, curved surfaces/material planning and physical qualification
are not completed by this config-only fingerprint. Separately observed Linux
run 36434944242 at 01865dcf built/saved dependencies and entered application/test
compilation at 15:34:10 UTC; it had not executed native tests at observation.
