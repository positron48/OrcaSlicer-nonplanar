# Первичные источники и статус проверки

Дата просмотра: 27 сентября 2026 года. База привязана к `v2.4.2` / `8500fcdccaa10b5099ac20d252af3a7c560046f1`, а не к изменяемой ветке main. Страница releases может содержать более поздние nightly; они не используются как база.

Просмотрены выбранные файлы/документы через веб. Это не полное чтение репозитория, локальная сборка, запуск тестов или печатное испытание. Прямое сетевое получение репозитория в среде подготовки не выполнялось успешно; локальная проверка git и сборки остаются Gate A. Приведённые ссылки — основание ограниченных утверждений об исходниках, не подтверждение предлагаемой архитектуры.

| ID | Источник | Для чего |
|---|---|---|
| O01 | https://github.com/OrcaSlicer/OrcaSlicer | Официальный upstream и AGPL-3.0 notice |
| O02 | https://github.com/OrcaSlicer/OrcaSlicer/releases/tag/v2.4.2 | Закреплённый релиз |
| O03 | https://github.com/OrcaSlicer/OrcaSlicer/commit/8500fcdccaa10b5099ac20d252af3a7c560046f1 | Commit релиза |
| O04 | https://github.com/OrcaSlicer/OrcaSlicer/blob/8500fcdccaa10b5099ac20d252af3a7c560046f1/src/libslic3r/ContourZ.cpp | Штатный ZAA; отдельный baseline, не новый planner |
| O05 | https://github.com/OrcaSlicer/OrcaSlicer/blob/8500fcdccaa10b5099ac20d252af3a7c560046f1/src/libslic3r/ExtrusionEntity.hpp | Текущие типы путей; проверить scaled coordinates и consumers |
| O06 | https://github.com/OrcaSlicer/OrcaSlicer/blob/8500fcdccaa10b5099ac20d252af3a7c560046f1/src/libslic3r/GCode.cpp | ZAA-сериализация, реальные координаты/экструзия и downstream filters |
| O07 | https://github.com/OrcaSlicer/OrcaSlicer/blob/8500fcdccaa10b5099ac20d252af3a7c560046f1/src/libslic3r/Format/STEP.cpp | Существующий OCCT-импорт и точность тесселяции |
| O08 | https://github.com/OrcaSlicer/OrcaSlicer/blob/8500fcdccaa10b5099ac20d252af3a7c560046f1/CMakeLists.txt | Флаги сборки и платформенные ограничения |
| O09 | https://github.com/OrcaSlicer/OrcaSlicer/blob/8500fcdccaa10b5099ac20d252af3a7c560046f1/tests/CMakeLists.txt | Catch2/CTest integration |
| O10 | https://github.com/OrcaSlicer/OrcaSlicer/blob/8500fcdccaa10b5099ac20d252af3a7c560046f1/src/slic3r/GUI/BackgroundSlicingProcess.cpp | Финализация, публикация и отправка |
| O11 | https://github.com/OrcaSlicer/OrcaSlicer/blob/8500fcdccaa10b5099ac20d252af3a7c560046f1/src/libslic3r/Format/bbs_3mf.cpp | 3MF project persistence; namespaced extension предложено |
| O12 | https://github.com/OrcaSlicer/OrcaSlicer/blob/8500fcdccaa10b5099ac20d252af3a7c560046f1/src/slic3r/GUI/GCodeViewer.cpp | Нативный preview и привязка к конечным движениям |
| O13 | https://github.com/OrcaSlicer/OrcaSlicer/blob/8500fcdccaa10b5099ac20d252af3a7c560046f1/src/libslic3r/PrintConfig.cpp | Регистрация конфигурации и resolved policy |
| O14 | https://github.com/OrcaSlicer/OrcaSlicer/blob/8500fcdccaa10b5099ac20d252af3a7c560046f1/src/libslic3r/Print.cpp | Slicing/export orchestration; точный hook требует local audit |
| O15 | https://github.com/OrcaSlicer/OrcaSlicer/blob/8500fcdccaa10b5099ac20d252af3a7c560046f1/src/libslic3r/PrintObject.cpp | Объектные steps, shell/regions и invalidation |
| O16 | https://github.com/OrcaSlicer/OrcaSlicer/blob/8500fcdccaa10b5099ac20d252af3a7c560046f1/src/slic3r/GUI/Tab.cpp | Нативные параметры/панели |
| S10 | https://github.com/flexible-collision-library/fcl | Кандидат для расстояний/CCD; пары и гарантии проверить |
| S11 | https://www.klipper3d.org/G-Codes.html | Поддержанные команды и модальности |
| S12 | https://www.klipper3d.org/Config_Reference.html | Оси, пределы движения/экструзии и настройки |
| S13 | https://github.com/Ultimaker/Ultimaker2 | Официальные детали/BOM как кандидат reference CAD |
| S14 | https://www.klipper3d.org/Bed_Mesh.html | Преобразования Z и fade |

Документация Klipper по приведённым адресам изменяемая. На этапе реализации зафиксировать соответствующие firmware commit/документацию и подтвердить совпадение с используемым принтером. Привязка к Orca commit не фиксирует версию прошивки.

Репозиторий Ultimaker 2 подтверждает наличие опубликованных деталей, а не комплектность готового профиля новой системы. Конкретные CAD-файлы/ревизии/сборка reference-профиля требуют отдельного аудита C03. В архив не включаются чужие исходники, CAD, бинарные зависимости или шрифты.

Требования, предложения алгоритмов, математические инварианты и оценки часов в документах — проектные решения. Для них источник истины — SPEC/ADR и будущая проверка, а не предположение, что это уже реализовано в Orca.
