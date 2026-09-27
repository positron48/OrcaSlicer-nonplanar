# Архитектура и ответственность модулей

## Основной конвейер

```text
Orca model + resolved presets + measured scene/head
        |
        v
Immutable JobSnapshot + compatibility decision
        |
        v
UpperSurfaceAnalyzer -> CapReservation -> Orca planar engine
        |                                     |
        |                         Planar semantic paths
        +--------------------+----------------+
                             v
                 DepositedBase reconstruction
                             v
          TransitionSolver -> CapPaths -> Order/Travel
                             v
               Volume + axis/flow motion limits
                             v
                     Typed MotionPlan
                             v
         Orca serialization and allowed final filters
                             v
                  Immutable candidate bytes
                             v
          Independent parser / material replay / checks
                             v
         ExportGate -> atomic publication + manifest
```

## Контракты модулей

| Модуль | Вход | Выход | Не имеет права |
|---|---|---|---|
| SnapshotBuilder | Все resolved входы | Неизменяемый снимок + fingerprint | Читать изменившийся preset посередине job |
| CompatibilityPolicy | Снимок | PASS/FAIL/UNKNOWN и причины | Молчаливо менять настройки пользователя |
| SurfaceAnalyzer | Исходная геометрия/ошибка | Patch masks/ownership/кандидаты | Считать один угол доказательством доступности |
| VolumePartition | Model + patches | Body/cap volumes + interface | Оставлять дубликат материала в теле |
| PlanarAdapter | Зарезервированное тело | Семантические пути Orca | Скрывать width/height/volume/пустоты |
| DepositionModel | Пути + материал | D_nominal/D_upper/D_lower | Создавать гарантированную опору из завышенной оболочки |
| TransitionSolver | Фактическая основа + target | Проходы/граничные условия | Поднимать сопло без изменения объёма/поддержки |
| PathPlanner | Допустимые поверхности | Конечные дорожки, seams | Подменять конечный след одной центральной линией |
| CollisionQuery | Инструмент/сцена/движение | Lower bound clearance + uncertainty | Возвращать PASS при неподдерживаемом CCD |
| OrderTravelPlanner | Дорожки + material state | Полный сериализуемый порядок | Считать обычный Z-hop доказательством |
| MotionLimiter | Объёмы/траектория/limits | Консервативные F и команды | Повышать firmware limits |
| GCodeAdapter | Typed plan | Конечный G-code candidate | Применять flow/ZAA correction повторно |
| Verifier | Байты + scene/profile manifest | Replay/report/evidence | Доверять planner-safe флагу |
| ExportGate | Отчёт + hashes + current revision | Разрешение публикации | Обходить UNKNOWN из GUI/CLI |

## Состояние материала и времени

План — упорядоченный поток событий, не массив «слоёв», отсортированный по Z. Непланарный сегмент может двигаться вверх или вниз, а разные слои иметь пересекающиеся диапазоны Z. Layer number допускается как label для пользователя, но не определяет порядок.

События различают print, travel, retract, unretract, heating/wait, fan и согласованное изменение ограничений. Pure-E purge требует отдельной разрешённой сцены осаждения; не прятать бесконечный объём у сопла. Подготовка/прайминг могут выполняться оператором до проверяемой части по отдельному контракту.

В основе планарные дорожки также моделируются в порядке выполнения: столкновение при входе в крышку зависит от реального положения периметров, а не абстрактного B-rep. Для производительности допускается иерархическая наружная/внутренняя оболочка с контролируемой ошибкой.

## Независимость verifier

Производственный verifier — отдельный parser/replay pipeline; он может разделять фундаментальные векторные типы, схемы профилей и геометрические библиотеки, но не готовое решение planner collision-free. Аналитические тесты геометрии и объёма должны использовать другой путь вычисления. На малых сценах — медленный эталон для сопоставления.

Sidecar roles/width не являются доказательством формы дорожки: verifier проверяет согласованность E и объёма, локального зазора, допустимой ширины и модели материала. Ошибочное, но подписанное тем же job_id поле sidecar должно выявляться тестом. Полноценная физическая CFD-модель не требуется; области неопределённости явно ограничиваются.

## Отмена и параллелизм

Тяжёлые расчёты не работают в GUI thread. Контексты worker содержат revision и cancel token. Результаты принимаются только для текущего snapshot. Shared mutable print state нельзя менять одновременно из GUI и solver; передача владения и область блокировок описываются в интеграционном ADR.

Timeout/memory limit/cancel не дают частично Verified задания. При недоступности отмены длительного OCCT/CCD-вызова допустим изолированный native worker process. Новый веб-сервис не вводится.

## Сохранность исходника

Оригинальная модель, её transforms и пользовательские presets неизменны. Body/cap partition — производные данные задачи. Undo/redo и reload STEP должны порождать новую revision, а не восстанавливать предыдущий safety badge.
