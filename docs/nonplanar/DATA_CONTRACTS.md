# Данные, единицы и сериализация

## 1. Координаты

Внутренние новые вычисления: миллиметры, секунды, мм³, радианы, конечные float64. Orca scaled integer coordinates конвертируются только именованными функциями на границе. Запрет на неявный cast scaled Z в мм. Бесконечность/NaN никогда не заменяются на 0 для продолжения печати.

Системы: `model_local`, `build_plate`, `machine_commanded`, `machine_physical`, `tool_local`. Сопло tool-local: начало в центре выходного отверстия на нижней плоскости; +Z к корпусу. Ориентация головы фиксирована относительно машины и не следует касательной.

Для MVP firmware transforms допускаются только как identity после доверенной подготовки либо как отдельно реализованный и протестированный фиксированный контракт. Предварительная механическая регулировка/пробинг не считается проверенным слайсером движением. Стол не обязан быть идеально плоским; при отключённом bed mesh допустимая остаточная неплоскостность должна быть измерена и вписана в first-layer budget.

## 2. MotionPlan (проектный typed IR)

```text
JobSnapshot:
  job_id, revision, schema_version, fingerprint
  source_geometry + transforms + tessellation_error
  resolved_print/material/printer_settings
  toolhead_profile + scene + firmware_contract
  algorithms + numeric_budget + baseline/fork commit

MotionEvent:
  event_id, sequence_index, kind, source_patch_id, nominal_layer_label
  start_xyz_mm, end_xyz_mm                 # absolute, nozzle opening position
  deposited_volume_mm3                     # only material actually deposited
  retract_or_unretract_filament_mm          # not deposition volume
  width_contract, gap_range, material_model_id
  speed_limit_mm_s, acceleration_contract
  support_provenance, deposition_contact_model_id
```

`kind` не выводится только из знака E: unretract не является новой дорожкой. На travel нет разрешённого contact exception. `nominal_layer_label` не меняет Z и не переставляет события.

После motion planning не допускается consumer, меняющий XYZ/E/order без аннулирования проверки. Выборочный layer preview не должен создавать новый файл для печати незаметным пропуском событий.

## 3. Объём и flow

`V_target` — объём согласованной модели дорожки; `A_f = π*d_filament²/4`. Если коэффициент подачи k используется, определить его смысл: `E_command = V_target/A_f * k`, а физическая реконструкция применяет согласованную откалиброванную модель delivered-volume. Не считать одновременно k дополнительным геометрическим объёмом и поправкой за недоподачу.

Для начала k=1 и калиброванный профиль — наиболее простой однозначный контракт. Если Orca filament flow_ratio уже применён на одном этапе, адаптер не применяет его второй раз. Ограничение расхода проверяется по принятому определению Q и по реальной команде E; доказательство не исчезает при k≠1.

Вертикальная ячейка: `V = integral_A h(x,y) dxdy`. Альтернативный контракт нормального сечения: `V = integral_L A_perp(s) ds3D`. Выбрать один основной, доказать согласованность на аналитических примерах и не перемножать две поправки наклона. Width по XY и по поверхности — разные величины.

## 4. Численный бюджет

Отдельно хранить import/tessellation error, path chord error, distance-query error, координатное округление, измерение инструмента, позиционирование и отклонение дорожки. Пример распределения чисто численного бюджета 0,05 мм: 0,01 import + 0,02 path + 0,01 distance + 0,01 transform/serialization. Это проектные лимиты, которые должны быть проверены; сумма включается в геометрический вывод. Нельзя считать каждый компонент допустимым по 0,05 и затем объявить общий допуск 0,05.

Верхняя цель chord error из SPEC 0,03 мм не обязана быть полностью использована, если другие компоненты исчерпывают общий бюджет. Аналитические fixtures без import error могут перераспределять бюджет только явным профилем.

DistanceResult: lower_bound_mm, bound_error_mm, required_clearance_mm, interaction_class, status/reason, witness и интервал параметра движения. Отсутствие witness у PASS не означает отсутствия обязательного bound.

## 5. 3MF и профиль

Предлагаемое расширение: `Metadata/nonplanar/project.json` с собственным schema_version, выбранным режимом, references/fingerprints, patch selections и embedded profile. Структуру сверить с обработчиком Orca. Примеры в `examples/` не предназначены для импорта stock Orca без кода расширения.

`profiles/generic_conservative_unverified.json` — **симуляционный формат нового модуля**, не готовый JSON preset Orca. Числа условные. Даже крупная оболочка не гарантирует охват произвольного реального обдува. `printer_measurement_template.json` содержит null до измерений; такой профиль блокирует экспорт.

Подтверждение профиля связывается с fingerprint геометрии, установленной комплектацией и датой/методом измерения. Изменение любой геометрии делает подтверждение устаревшим. Schema validation не подтверждает правильность измерений.

## 6. Конечное задание и hashes

1. Создать snapshot fingerprint из канонического JSON: UTF-8, sorted keys, без NaN/Infinity; поля timestamp/log и секреты не входят. Для межъязыковых чисел определить единый round-trip/canonicalization тест до реализации.
2. Сериализовать candidate G-code и завершить все разрешённые фильтры. Не включать собственный hash файла внутрь файла.
3. Вычислить SHA-256 окончательных байтов и manifest с этой ссылкой, hashes inputs/settings/scene/software.
4. Verifier читает конечные байты заново, проверяет manifest и заново реконструирует движения/материал. Report ссылается на candidate hash и manifest hash. Report не используется как источник параметров для собственной проверки.
5. ExportGate сверяет current revision, report, fingerprint и фактические bytes; публикует набор с commit marker. При записи на внешнее устройство проверяет скопированные bytes.

Hash обнаруживает изменение, но не доказывает геометрическую правильность и не защищает от намеренной подделки владельцем файлов. Криптографическая сертификация и защита от пользователя не входят в объём.

## 7. Статусы

Check = PASS | WARN | FAIL | UNKNOWN. Отдельно execution = NOT_RUN | RUN | SKIPPED | ERROR. NOT_RUN не равен PASS; UNKNOWN в обязательной проверке блокирует выдачу. Export decision = ALLOW | BLOCK; ALLOW требует полный список mandatory checks, измеренный профиль и известные предусловия.

`examples/validation-unknown.json` — намеренно блокирующий пример, не результат прогона слайсера. Наличие schemas обеспечивает форму данных, а не истинность заключения.
