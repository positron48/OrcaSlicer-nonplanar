# Этапы разработки и контрольные результаты

Все задачи **NOT_STARTED**. Машиночитаемый DAG — `backlog.json`; источником численных критериев остаётся SPEC. Часы в SPEC — предварительный совокупный бюджет, не обещание выполнить каждую строку в фиксированный срок.

Сначала небольшой проход Gate A: закреплённая база, stock baselines, контракты, геометрические/объёмные риски. Затем Gate B: рабочий ограниченный гибрид внутри Orca. Gate C завершает весь экспериментальный объём. Выполненный P0/P1 не выдаётся за готовое P2.

## Gate A — доказать архитектуру

| ID | Задача | Зависимости | Обязательный результат |
|---|---|---|---|
| A01 | Закрепить и собрать stock Orca | — | Локальный SHA audit; build logs; непустой CTest; изолированная dev-установка |
| A02 | Карта конвейера и baseline OFF/ZAA | A01 | Документированный call graph, immutable stock test data и semantic diff |
| A03 | Математические контракты и typed IR | A01 | ADR координат/толщин/объёма/материала; типы, analytic tests |
| A04 | Tool geometry и аналитический sweep | A03 | Конечный торец, асимметрия, continuous bound; positive/negative oracles |
| A05 | Spike переходной области | A03, A04 | Допустимый/недопустимый переход; proof of volume budget, без печати |
| A06 | Spike native serialization и parser | A02, A03 | Round-trip маленького IR, independent parser, no ZAA reuse shortcut |
| A07 | Протокол измерений и первый scene contract | A03 | Simulation-only profile, перечень неизвестных измерений, операторская инструкция |
| A08 | Gate A review и benchmark | A04, A05, A06, A07 | Измерения CPU/memory, открытые риски, выбор native hooks, пересчитанный бюджет |

## Gate B — сквозной MVP в Orca

| ID | Задача | Зависимости | Обязательный результат |
|---|---|---|---|
| B01 | Режим, policy и resolved configuration | A08 | Namespaced config; mutual exclusion; immutable policy snapshot |
| B02 | STL validation и source preservation | B01 | Импорт через Orca, отчёт ограниченного repair, исходник неизменён |
| B03 | Выделение одной верхней области | B02, A04 | ROI, наклон/кривизна, объяснимый отказ и площадь обработки |
| B04 | Body/cap partition и native planar adapter | B03, A05 | Body создан Orca; нет повторного top fill; dense interface и semantic paths |
| B05 | Последовательная модель материала | B04 | D_nominal/upper/lower; previous/current segments; bounded contact model |
| B06 | Transition solver | B05, A05 | Реальная ступенчатая опора, thickness limits, шов, отказ без опоры |
| B07 | Непланарные контуры и infill | B06 | Конечный след, adaptive segmentation и направление проходов |
| B08 | Непрерывные collisions для полных путей | B05, B07, A04 | Голова/сцена/материал, witness, bound budget, fail-closed |
| B09 | Order, travel, entry/exit и парковка | B08 | Полный порядок без downstream reordering; проверенные переезды |
| B10 | Объём, flow и motion limits | B07, B09 | Однозначный V→E, full Z/axis/Q bounds, firmware constraints |
| B11 | Нативный exporter и финальные фильтры | B10, A06 | Конечный candidate; no double Z/E, no hidden wipe/reset |
| B12 | Независимый verifier | B11 | Parser/replay последнего текста, material/limits checks, independent oracles |
| B13 | Единый gate и immutable job state | B12, B01 | Нет обхода file/CLI; cancellation/stale, network off, snapshot/hash binding |
| B14 | Минимальный нативный UI и CLI | B13 | Управляемый сквозной путь в Orca, diagnostics и replay по движению |
| B15 | Операторские первые испытания и Gate B | B14, A07 | Измеренный профиль и validated файл; клин/пологий купол; протокол, не автозапуск |

## Gate C — законченный экспериментальный объём

| ID | Задача | Зависимости | Обязательный результат |
|---|---|---|---|
| C01 | STEP units/accuracy/assemblies | B14 | Квалифицированный native STEP, chord budget и invalidation |
| C02 | Несколько крышек и сложные границы | B14 | Купола/стена/отверстие/седло; геометрия сохранена, порядок проверен |
| C03 | Полные профили и audited reference CAD | B14, A07 | Параметрический профиль, provenance/coverage и проверенный CAD reference; без фиктивных измерений |
| C04 | Генератор STL/SVG калибров | C03 | Сечения/рамка, scales, fit budget и hash; холодная примерка |
| C05 | Полный GUI и sequential preview | C02, C04 | Native panels, head/material/witness views, exported path identity |
| C06 | 3MF persistence/migration и payload gate | C01, C03 | Round-trip, unknown schema, reload, no unsafe embedded G-code |
| C07 | Точная sidecar и verifier consistency | C02, B12 | Независимая сверка volume/width/gap, schema and mandatory registry |
| C08 | Полная аналитическая/negative/property матрица | C01, C02, C06, C07 | Fixtures/oracles/mutations; без all-reject; evidence всех test IDs |
| C09 | Ресурсы, кэш, атаки и atomic recovery | C06, C07 | Memory/cancel/security tests, commit marker и финальные hashes |
| C10 | Сборки/изоляция/лицензии и upstream discipline | C05, C06, C09 | Две native платформы, отдельный config, release packaging и notices |
| C11 | Performance и точность на большом примере | C08, C09 | Измеренный benchmark с полным verifier и численным бюджетом |
| C12 | Физическая квалификация серии | B15, C02, C04, C07 | Три формы × три последовательных успеха; baseline OFF/ZAA/hybrid и размеры |
| C13 | Gate C и комплект передачи | C08, C10, C11, C12 | Все обязательные evidence, известные ограничения, исходники/сборки/доки/профили |

## Правила gates

Gate A не закрывается импортом и красивым preview. Нужны: реальный build, baseline, аналитические finite-tip/continuous-collision/volume проверки и один допустимый/недопустимый переход. Работа с реальной головой может идти параллельно; отсутствие измерений блокирует физическую печать, но не синтетические unit tests.

Gate B требует полного пути и минимального независимого verifier до первой печати. Отсутствующая физическая квалификация явно оставляет эту часть Gate B NOT_RUN; программный результат можно передать как software candidate, не объявляя весь gate завершённым.

Gate C требует все обязательные функции/тесты, платформы и серии повторяемости. Нельзя «закрыть» unavailable test удалением строки из CSV или переносом обязательного требования в optional.

Для каждой задачи использовать `templates/TASK.md`; формат evidence — `templates/TEST_RESULT.md`. Первое готовое задание агенту — `prompts/01_bootstrap.md`.
