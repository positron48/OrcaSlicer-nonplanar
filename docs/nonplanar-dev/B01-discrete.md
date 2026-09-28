# B01 native discrete compatibility values

Invariant: each declared boolean, integer and enum compatibility requirement
matches the native type and value, not a serialized string or a borrowed enum
label. Ten original constraints now form a typed machine-readable registry.
Accepted labels are diagnostics from that registry; rejected values report the
native integer, missing field or wrong type without serializing unchecked enums.

The original code accepted ConfigOptionString substitutions for all ten values
and generic enum dictionaries mapping an allowed label to the wrong integer.
It also interpreted a single-element string vector as an OFF/guarded mode.
Three regression cases demonstrated 20 failed assertions before the fix (exit
42). Mode routing now accepts OFF only as an actual native scalar string; every
other present type requests guarded rejection and resolves to Invalid. Enum
policy reads the actual pinned numeric meanings used by the native engine.
Sparse model/material/volume/layer config inspection applies the same rules.

A fourth case covers an out-of-range negative typed enum without invoking its
unsafe native serializer, plus sparse source type rejection and valid correction.
Registry tests compare all native definitions and enum label mappings. This
caught the initial incorrect FuzzySkinType::None choice: in this pinned Orca it
means painted-only, while disabled_fuzzy is FuzzySkinType::Disabled_fuzzy (5).
The corrected implementation preserves the prior requirement, not a broader
fuzzy-skin exception. Both initial and intermediate failures remain in evidence.

Final macOS ARM64 Release app and native targets build with exit 0. Twenty-three
B01 cases pass (2906 assertions, NoAssertions enabled). Fresh selected CTest
executes 142/142 without skips or disabled tests. All six final OFF/ZAA captures
match stock motion/G-code, with only the documented timestamp normalization and
additive nptop_mode=off resolved setting. Commands, exit codes, XML and hashes
are in evidence/B01-discrete. OFF source settings remain unchanged.

This closes type/enum ambiguity for the existing bounded rules. It is not full
compatibility coverage, raw unknown-import provenance, profile qualification,
a Linux pass or a complete immutable job. All guarded output is still blocked
until the implemented pipeline and independent verifier can authorize it.
