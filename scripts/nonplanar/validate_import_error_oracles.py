#!/usr/bin/env python3
"""Independent exact rational proof of the native import-error test constants."""
from fractions import Fraction
import json
import struct

CASES = [
    ('0.1', 0x3DCCCCCD, -29), ('-0.1', 0xBDCCCCCD, -29),
    ('0.5', 0x3F000000, None), ('-1.25e2', 0xC2FA0000, None),
    ('1e-45', 1, -150), ('1e-100', 0, -332),
    ('1.000000059604644775390625', 0x3F800000, -24),
    ('10000.0001', 0x461C4000, -13),
    ('0.1000000000000000000000000001', 0x3DCCCCCD, -29),
    ('0.0000000001', 0x2EDBE6FF, -59),
    ('1.401298464324817e-45', 1, -203), ('-0', 0x80000000, None),
]


def main():
    results = []
    for decimal, bits, exponent in CASES:
        parsed = struct.unpack('!f', struct.pack('!I', bits))[0]
        error = abs(Fraction(decimal) - Fraction(parsed))
        if exponent is None:
            if error != 0:
                raise AssertionError((decimal, error))
        elif not Fraction(2)**(exponent-1) < error <= Fraction(2)**exponent:
            raise AssertionError((decimal, error, exponent))
        results.append({'decimal': decimal, 'binary32_bits': f'{bits:08x}',
                        'exact_error_mm': str(error), 'upper_power_of_two': exponent})
    print(json.dumps({'status': 'PASS_EXACT_NUMERIC_ORACLE_CONSTANTS_ONLY',
                      'cases': results}, indent=2))


if __name__ == '__main__':
    main()
