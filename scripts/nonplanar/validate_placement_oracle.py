#!/usr/bin/env python3
"""Exact rational oracle for the native placement regression's known corner."""
from fractions import Fraction
import json
import struct

offset = Fraction.from_float(0.1)
errors = []
for coordinate, encoded in [(15, 7916749), (10, 5295309), (4, 8598323)]:
    divisor = 2097152 if coordinate == 4 else 524288
    expected_binary32 = Fraction(encoded, divisor)
    actual_binary32 = Fraction.from_float(struct.unpack("<f", struct.pack("<f", coordinate + 0.1))[0])
    assert actual_binary32 == expected_binary32
    errors.append(abs(Fraction(coordinate) + offset - expected_binary32))
total = sum(errors, Fraction(0))
assert total == Fraction(30923764531, 36028797018963968)
assert Fraction.from_float(30923764531.0 / 36028797018963968.0) == total
print(json.dumps({"status": "PASS", "binary64_translation": str(offset),
                  "corner": [15, 10, 4], "axis_errors": [str(error) for error in errors],
                  "l1_error": str(total), "l1_error_mm": float(total)}, indent=2))
