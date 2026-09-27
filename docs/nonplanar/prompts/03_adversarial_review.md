# Независимое review геометрии и интеграции

Проверь заданный diff как критический reviewer. Не используй вывод автора «безопасно» в качестве доказательства. Прочитай SPEC, DATA_CONTRACTS, SAFETY_AND_VERIFICATION и относящиеся test IDs.

Ищи: неверный scaled→mm; absolute/relative Z; двойной E multiplier; утерянный порядок; восстановленный Orca top shell; endpoint-only collision; неверный знак D_upper/D_lower; широкое contact exception; потерянный материал retract/unretract; скрытый reset Z/wipe; unknown macro; filter после проверки; stale callback; file/CLI/3MF/cache обход; отчёт без mandatory check; hash-only verifier; изменение user presets.

Для найденного риска построй минимальный контрпример и independent expected result. Проверь, что положительные модели реально получают непланарный ROI. Отдельно проверь OFF/ZAA regression. Не исправляй провал удалением expected или снижением margin. Выдай severity, точные файлы/строки, воспроизведение и критерий исправления.
