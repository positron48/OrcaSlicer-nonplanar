# B02: bound decimal-to-native coordinate conversion

Invariant: a parsed floating-point coordinate cannot stand in for an exact
decimal source without a reported bound. A missing/unsupported bound is UNKNOWN,
never zero. This extends the internal byte-snapshot API; it does not change the
ordinary Orca importer, establish solid containment, or approve hybrid output.

For each ASCII vertex token, the module reads an exact signed rational using
bounded multiprecision integers. It independently decomposes the actual native
binary32 result into a signed significand and a power of two. The absolute
decimal/binary difference is calculated with integer products. Integer comparison
selects the smallest enclosing power of two; no floating-point approximation of
that rational or assumption about scanf's rounding is needed.

The per-coordinate domain is a decimal token at most 80 bytes, at most 64 digits
in its significand and an explicit exponent between -100 and +100. Hexadecimal,
nonfinite, malformed or larger-domain tokens do not obtain a bound. The resulting
integer work and exponent range are finite. Power-of-two bounds are exactly
representable as normal IEEE binary64 values. Per-vertex coordinate bounds are
summed with outward rounding under the existing strict floating-point contract.
An exactly represented vertex retains an exact zero bound. Binary STL's native
binary32 copy has zero conversion error relative to those encoded coordinates.

The maximum vertex L1 displacement encloses every vertex's Euclidean displacement.
A point with the same barycentric coordinates in corresponding source/parsed
triangles moves no more than the maximum of their vertex bounds. Therefore this
also bounds displacement between the corresponding triangle surfaces, including
their interiors. The already checked exact oriented-triangle repair comparison
preserves this correspondence after native repair. This argument does not prove
that an exact decimal triangle soup and its quantized mesh share solid topology;
it is not a solid occupancy or model-to-machine transform certificate.

The snapshot result now carries an optional source_error_upper_mm. A geometry
result is accepted only when that bound was established; a downstream policy
still has to charge it to its import budget. This is not a new arbitrary margin
or a claim that all rounded inputs meet the complete job budget. Source-path
capture, transforms, hard worker limits, persistence and job qualification remain
pending. No persistent schema is changed by this internal API field.

Tests compare native results with twelve independently checked exact rational
oracles, including 0.1 (error exactly 1/671088640), a rounding midpoint, binary32
subnormal values and decimal underflow. The Python Fraction checker proves the
test constants without importing native implementation code. End-to-end native
ASCII import reports the nonzero 0.1 bound; an otherwise parseable hex-coordinate
input is UNKNOWN. Invalid grammar and out-of-domain numbers cannot claim zero.
Independent critical review and physical qualification remain pending.

## Observed validation

macOS ARM64 Release native test targets built successfully. Three focused cases
pass 35 assertions with NoAssertions; all twelve exact-rational oracle constants
pass their independent Python check. Selected CTest executes 110/110 with no
skips/failures. Exact commands, exit codes, native XML, compiler/platform and
source/binary hashes are in evidence/B02-error. The normative bundle audit passes.
No fresh application build or OFF/ZAA capture was run: the modified guarded import
API has no production caller and shared stock import/slicing/writer paths are
unchanged since the six successful B02-snapshot captures. Linux CI still builds
an earlier commit; no Linux, ASan, review or physical result is inferred.
