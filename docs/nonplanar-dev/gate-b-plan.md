# Gate B implementation and completion audit

Active objective: implement the entire B01–B15 scope. A bounded primitive or a
green unit suite is evidence for its own contract, not completion of a B task.
The normative backlog, SPEC and acceptance matrix remain unchanged.

| Task | Required deliverable | Current authoritative state | Remaining work |
|---|---|---|---|
| B01 | Namespaced config; mutual exclusion; immutable policy snapshot | Policy.cpp, PrintConfigSnapshot, NativeInputSnapshot/Print revision; native/CLI rejection tests | Bind complete job dependencies and plate invalidation; finish explicit compatibility registry and UI/import coverage. |
| B02 | Импорт через Orca, отчёт ограниченного repair, исходник неизменён | StlImport/StlWorker/MeshPlacement; owned pre-apply source/STL placement | Bind original file/units/repair and native placement to the actual job/GUI plate; enforce hard resource containment; test source preservation through partition and persistence. |
| B03 | ROI, наклон/кривизна, объяснимый отказ и площадь обработки | UpperProjection; nominal masks, affine footprint/height checks | Qualified geometry error, curved patches, finite footprint, ROI/tool access and explained treatment area in the actual job. |
| B04 | Body создан Orca; нет повторного top fill; dense interface и semantic paths | A05 experiment, owned exact partition and private native whole-body semantics | Full dense edge/seam domain beyond the proven interior plane, volumetric seam, wider native geometry/roles and job/worker binding. |
| B05 | D_nominal/upper/lower; previous/current segments; bounded contact model | Owned constant-flux prefixes, separate envelopes, actual native body and ordered later-cap binding, per-event joins and protected whole run/event union boxes | Full contact/CCD qualification, wider union domains and actual job/replay integration. |
| B06 | Реальная ступенчатая опора, thickness limits, шов, отказ без опоры | Actual-prefix first gap/integral, affine allocation, complete flat-floor body interface, local later multi-run support and mandatory actual affine normal-ray band | Stepped/curved interface, complete shoulders/seam, measured contact and complete paths. |
| B07 | Конечный след, adaptive segmentation и направление проходов | Exact local union optimization under unchanged budgets; protected cap end/width replans with measured original fill/voids; exact body/active-cap assembly; local later bead and ordered actual later journal reused for a third original surface under unchanged limits | Qualified seam/excess, complete later path/layer/cap construction, general normal/contact qualification, alternating infill and adaptive face/curve subdivision in the actual job. |
| B08 | Голова/сцена/материал, witness, bound budget, fail-closed | Protected complete declared simulation head/static scene/actual old-current Upper composition on the same original event with shared budgets, coverage/height bounds and witnesses | Compose complete measured head/material/static scene, qualify deposition contact and applicability; bind whole job/order/replay. |
| B09 | Полный порядок без downstream reordering; проверенные переезды | Protected replacement of one original Travel by complete lift/transfer/descent, full head/static/actual prefix checks and shared budgets; exact material/pressure/order preservation; analytical positives and native UNKNOWN exit retained | Qualify full native contact/access, select admissible heights, preserve complete job/downstream order and verify all entry/exit/travel/prolog/parking/end motions; reject blocked low caps. |
| B10 | Однозначный V→E, full Z/axis/Q bounds, firmware constraints | Protected whole-journal full-stop linear plan; all XYZ/Cartesian-CoreXY drive/E/Q/cross-section/acceleration/pressure/event-rate bounds; exact dose/pose/order preservation; native 2218 rows | Bounded native candidate now expresses stops/global acceleration/dwell; complete final-byte verifier, actual transform/flow/contact and complete cap-volume/job contracts remain. |
| B11 | Конечный candidate; no double Z/E, no hidden wipe/reset | Protected complete native bytes/hash/policy/event map with explicit XYZ/E/pressure/dwell/full stops; final native 2218 rows | Audited final filters, complete prolog/end motions, native job binding and final geometry/rate/time verification. |
| B12 | Parser/replay последнего текста, material/limits checks, independent oracles | Independent exact-rational final-byte axis/drive/E/Q/acceleration/event rates, enclosed ideal time/dose/final pressure state; separate headless diagnostic; all 2218 native rows | Complete final-byte material/contact/support/dose uncertainty, actual transforms/flow qualification, complete route/job integrity and mutation witnesses. |
| B13 | Нет обхода file/CLI; cancellation/stale, network off, snapshot/hash binding | Guarded exports currently always blocked | Immutable job/revision state machine, mandatory checks, hash binding and atomic publication for every route; preserve no-network behavior. |
| B14 | Управляемый сквозной путь в Orca, diagnostics и replay по движению | Native diagnostic STL CLI only | Native UI/CLI integrated full pipeline, explained failures, treatment area and final replay preview. |
| B15 | Измеренный профиль и validated файл; клин/пологий купол; протокол, не автозапуск | Operator record is UNCONFIRMED; physical tests NOT_RUN | Measured installed U1 profile, bed/firmware/material prerequisites, planar reference and observed wedge/dome runs. Operator evidence is mandatory. |

## Acceptance evidence

Every referenced ID must be audited at its full normative scope before task
completion. Native primitives with the same ID do not substitute for complete
pipeline integration or physical evidence.

### B01

- ORC-04: Взаимоисключение ZAA и hybrid. Expected: REJECT конфликт либо явный документированный выбор; никогда двойной проход. Oracle: Trace вызовов; final geometry/E.
- ORC-10: Object override не обходит policy. Expected: REJECT по фактическому resolved значению. Oracle: Самостоятельно собранная effective-config таблица.
- ORC-35: Unknown geometry-affecting setting. Expected: REJECT до аудита; UI не скрывает причину. Oracle: Явная compatibility registry.

### B02

- IMP-01: STL: binary and ASCII. Expected: ACCEPT: одинаковые размеры, объём и нормализованная геометрия в допуске. Oracle: Аналитический объём блока; сравнение независимых импортов.
- IMP-03: Открытая и самопересекающаяся сетка. Expected: REJECT либо явный ограниченный repair с отчётом; не тихий PASS. Oracle: Заранее известные дефекты исходной сетки.
- IMP-04: NaN и вырожденные грани. Expected: REJECT неконечных координат; вырожденные элементы обрабатываются по явной политике. Oracle: Проверка диагностик и отсутствия экспортного G-code.
- ORC-30: Исходная модель не мутирует. Expected: Оригинал сохранён; body mesh только derived data. Oracle: Hash исходной mesh/B-rep/transforms.

### B03

- CAP-01: Плоский блок. Expected: ACCEPT; Z крышки остаётся постоянным; непланарный эффект не выдуман. Oracle: Аналитическая плоскость и размеры.
- CAP-02: Пологий клин. Expected: ACCEPT заявленной внутренней области; не all-planar fallback. Oracle: Аналитическая плоскость; заранее сохранённая ожидаемая маска.
- CAP-03: Недоступный склон. Expected: strict: REJECT; hybrid: явно показанный планарный возврат. Oracle: Независимая инструментальная проверка и площадь fallback.
- CAP-04: Цилиндрический валик. Expected: ACCEPT известной внутренней маски; точность дуги в допуске. Oracle: Аналитический цилиндр и chord error.
- CAP-05: Пологий сферический сегмент. Expected: ACCEPT заданной площади; послойная опора и объём корректны. Oracle: Аналитическая сфера; независимое интегрирование объёма.
- CAP-13: Внутренняя скрытая грань. Expected: Не классифицируется внешней доступной крышкой. Oracle: Известная топология CAD и вертикальная видимость.
- CAP-14: Микроостров и тонкий выступ. Expected: Явный fallback/отказ без потери геометрии. Oracle: Исходная маска; контроль исчезнувшей площади/объёма.

### B04

- ORC-09: Orca не восстанавливает крышку. Expected: Нет дублированного объёма или пропавших стен. Oracle: Разложение model=body+caps; sampled volume oracle.
- INT-04: Редкое заполнение под крышкой. Expected: Добавить рассчитанную плотную подложку или отказать; не считать весь CAD сплошным. Oracle: D_lower и положение реально уложенных дорожек.
- INT-06: Планарный/непланарный шов. Expected: Нет открытой щели, двойного объёма и потери целевой границы. Oracle: Контрольные объёмные сечения, не только центральные линии.

### B05

- MAT-01: Три представления материала. Expected: Коллизии используют D_upper; опора — D_lower; CAD не подставляется вместо них. Oracle: Сцена, где один общий inflated volume даёт ложную опору.
- MAT-02: Свежая соседняя дорожка. Expected: REJECT или допустимый пересчитанный порядок. Oracle: Пошаговый replay и локализация времени первого контакта.
- MAT-03: Материал внутри длинного сегмента. Expected: Постепенное добавление; нельзя добавлять весь сегмент только в конце. Oracle: Эталонное мелкошаговое воспроизведение с учётом погрешности.
- MAT-04: Слишком широкое contact exception. Expected: REJECT; материал не вырезается из сцены глобальным радиусом. Oracle: Точная тестовая геометрия зоны исключения.
- GCD-09: Retract/unretract без фантомной дорожки. Expected: Материал не добавлен ошибочно при восстановлении давления. Oracle: Независимый event/retraction state.

### B06

- INT-01: Первый проход на плоской основе. Expected: ACCEPT только при допустимом зазоре во всех точках следа. Oracle: Аналитическая плоскость опоры и зазор.
- INT-02: Ступенчатая основа. Expected: Нет слишком большого/малого первого зазора; реальная опора не заменена f-t. Oracle: Независимое восстановление подложки из траекторий.
- INT-03: Неверный вертикальный offset. Expected: REJECT: контакт или недостаточная опора; не автоматическое поднятие без пересчёта. Oracle: Известная величина внесённого смещения.
- INT-05: Пересечение соседних проходов. Expected: REJECT; толщина не может стать отрицательной. Oracle: Аналитическая разность поверхностей.
- INT-06: Планарный/непланарный шов. Expected: Нет открытой щели, двойного объёма и потери целевой границы. Oracle: Контрольные объёмные сечения, не только центральные линии.

### B07

- CAP-02: Пологий клин. Expected: ACCEPT заявленной внутренней области; не all-planar fallback. Oracle: Аналитическая плоскость; заранее сохранённая ожидаемая маска.
- CAP-04: Цилиндрический валик. Expected: ACCEPT известной внутренней маски; точность дуги в допуске. Oracle: Аналитический цилиндр и chord error.
- CAP-05: Пологий сферический сегмент. Expected: ACCEPT заданной площади; послойная опора и объём корректны. Oracle: Аналитическая сфера; независимое интегрирование объёма.
- VOL-02: Вертикальная и нормальная толщина. Expected: Нормальное расстояние согласовано с соглашением; объём не пересчитан дважды. Oracle: Для параллельных плоскостей h_n=h_z*cos(theta).

### B08

- GEO-03: Поперечный наклон. Expected: Задевание обнаружено даже при постоянном Z вдоль маршрута. Oracle: Аналитическое расстояние торца до плоскости.
- GEO-04: Непрерывная проверка. Expected: REJECT сегмента; проверка только концов не проходит. Oracle: Точный swept box/segment intersection.
- GEO-05: Обдув касается стенки. Expected: REJECT с ID компонента и препятствия. Oracle: Аналитическое пересечение боксов.
- GEO-06: Конус и плечо. Expected: REJECT с высотой контакта. Oracle: Расстояние до аналитического конуса/ступенчатого профиля.
- GEO-07: Пограничный зазор. Expected: Нельзя PASS внутри непроверенного интервала; UNKNOWN блокирует экспорт. Oracle: Независимая интервальная/аналитическая оценка.
- GEO-08: CCD: неподдерживаемая пара. Expected: UNKNOWN или консервативная замена; не PASS по умолчанию. Oracle: Зафиксированная матрица возможностей геометрической библиотеки.
- MAT-02: Свежая соседняя дорожка. Expected: REJECT или допустимый пересчитанный порядок. Oracle: Пошаговый replay и локализация времени первого контакта.
- MAT-03: Материал внутри длинного сегмента. Expected: Постепенное добавление; нельзя добавлять весь сегмент только в конце. Oracle: Эталонное мелкошаговое воспроизведение с учётом погрешности.

### B09

- MOV-01: Переезд через выступ. Expected: Проверенный lift-travel-descend либо отказ; все три части проверяются. Oracle: Непрерывное пересечение с фиксированной геометрией.
- MOV-02: Нельзя подняться. Expected: Не считать Z-hop автоматически безопасным. Oracle: Аналитическая сцена подъёма.
- MOV-03: Граница и Z-max. Expected: REJECT превышения или допустимый альтернативный маршрут. Oracle: Пределы профиля и модель препятствия.
- MOV-06: Пролог и окончание. Expected: Все собственные движения проверены; доверенный homing отделён в отчёте. Oracle: Replay полного файла, не только непланарной части.
- ORC-08: Порядок не потерян downstream. Expected: Экспортер не сортирует заново и не переворачивает проверенный путь. Oracle: Зафиксированная sequence и material replay.
- ORC-36: Неплановый сброс Z/wipe. Expected: Нет непроверенного reset/wipe; либо новый полный verify. Oracle: Полный replay, включая end/parking.

### B10

- VOL-01: Объём на горизонтали и наклоне. Expected: Одинаковый рассчитанный V; E=V/(pi*d_f^2/4) с заданной поправкой. Oracle: Аналитический интеграл; тест против ошибочного множителя 3D/XY.
- VOL-02: Вертикальная и нормальная толщина. Expected: Нормальное расстояние согласовано с соглашением; объём не пересчитан дважды. Oracle: Для параллельных плоскостей h_n=h_z*cos(theta).
- VOL-03: Филамент и расход. Expected: E обратно пропорциональна площади филамента; Q соответствует V/time. Oracle: Аналитические отношения и единицы.
- MOV-04: Ограничение Z при наклоне. Expected: Подача ограничена по Z; аналогично проверяются другие оси и расход. Oracle: v_z=v*abs(dz/ds); независимый расчёт лимитов.
- MOV-05: Предел ускорения и короткие сегменты. Expected: Снижение скорости/явный отказ по бюджету; не игнорирование оси Z. Oracle: Ограничения принятой модели движения и benchmark частоты сегментов.
- ORC-07: Flow применён ровно один раз. Expected: E соответствует V-контракту, без повторной ZAA/flow-поправки. Oracle: Аналитический объём и независимый E replay.

### B11

- ORC-06: Нет повторного nominal Z. Expected: Экспортированные absolute XYZ равны IR; нет doubled Z. Oracle: Независимый parser и аналитические точки.
- ORC-07: Flow применён ровно один раз. Expected: E соответствует V-контракту, без повторной ZAA/flow-поправки. Oracle: Аналитический объём и независимый E replay.
- ORC-11: Фильтр изменил финальный кандидат. Expected: Проверяется результат фильтра; после проверки изменение аннулирует gate. Oracle: Byte hash и independent motion replay.
- ORC-36: Неплановый сброс Z/wipe. Expected: Нет непроверенного reset/wipe; либо новый полный verify. Oracle: Полный replay, включая end/parking.
- GCD-06: Округление создаёт контакт. Expected: Итоговый файл отвергнут либо точность пересчитана и проверка повторена. Oracle: Независимый анализ именно текстовых координат.
- GCD-08: Нулевой сегмент/чистая экструзия. Expected: Явная обработка/отказ; нельзя бесконтрольно выдавить пластик на месте. Oracle: Длина сегмента и extrusion-only ограничения.

### B12

- GCD-01: Абсолютные XYZ и относительная E. Expected: Одинаковые физические перемещения в IR и повторном разборе файла. Oracle: Независимый парсер диалекта Klipper.
- GCD-02: Внесённый M82/G92. Expected: Обнаружить несогласованную экструзию/состояние; не доверять комментариям. Oracle: Независимый state machine и известные накопленные E.
- GCD-03: Повышенные E и F. Expected: REJECT; лимиты прошивки не повышаются автоматически. Oracle: Аналитический расход и осевые ограничения.
- GCD-04: Неизвестный макрос. Expected: UNKNOWN/REJECT; экспорт не считается проверенным. Oracle: Allowlist команд и явная граница доверия.
- GCD-05: Bed mesh и fade. Expected: Проверяется преобразованная траектория либо конфигурация отвергается. Oracle: Независимая реализация согласованного поднабора компенсаций.
- GCD-06: Округление создаёт контакт. Expected: Итоговый файл отвергнут либо точность пересчитана и проверка повторена. Oracle: Независимый анализ именно текстовых координат.
- GCD-07: Изменённый файл после проверки. Expected: Hash mismatch; старый отчёт неприменим. Oracle: SHA-256 всех входов и результата.
- GCD-09: Retract/unretract без фантомной дорожки. Expected: Материал не добавлен ошибочно при восстановлении давления. Oracle: Независимый event/retraction state.
- GCD-10: Некорректный числовой синтаксис. Expected: REJECT; no default-to-zero motion. Oracle: Фиксированная grammar и negative corpus.
- GCD-11: Неконтролируемый pure-E purge. Expected: REJECT/UNKNOWN; не потерять объём и препятствие. Oracle: Объём команд и scene contract.
- ORC-27: Согласованные hashes не скрывают дефект. Expected: FAIL geometry, не PASS integrity-only. Oracle: Аналитический witness столкновения.

### B13

- SYS-01: UNKNOWN закрывает экспорт. Expected: Нет конечного G-code задания; есть диагностический отчёт. Oracle: Проверка выходного каталога и статусов.
- SYS-02: Атомарный экспорт и отмена. Expected: Частичный файл не представлен как готовое задание. Oracle: Fault injection и проверка имён/manifest.
- SYS-05: Кэш и профиль. Expected: Зависимые кэши и отчёт аннулируются. Oracle: Manifest dependency graph.
- ORC-12: Запоздалый callback. Expected: Старый результат STALE, export закрыт. Oracle: Revision/state-machine oracle.
- ORC-13: Переключение режима со старым кэшем. Expected: Старый candidate не становится разрешённым; нужен новый slice. Oracle: Текущий mode fingerprint и отсутствие публикации.
- ORC-15: Все зависимости в ключе. Expected: Каждое значимое изменение инвалидирует affected stage/report. Oracle: Зависимости snapshot и повторное выполнение.
- ORC-20: GUI export закрыт. Expected: Нет финального G-code, даже вне enabled кнопки. Oracle: Файловые артефакты и единый policy decision.
- ORC-21: CLI export закрыт. Expected: Ненулевой exit code; no final file. Oracle: Process exit + directory snapshot.
- ORC-23: Сетевая отправка hybrid запрещена. Expected: Нет сетевой передачи/старта для hybrid; OFF не затронут. Oracle: Mock transport call count=0.
- ORC-25: Внешний postprocessor из профиля. Expected: Не исполняется; конфликт диагностируется. Oracle: Sentinel отсутствует; process execution audit.
- ORC-26: Отсутствующая mandatory проверка. Expected: BLOCK, даже если все оставшиеся checks PASS. Oracle: Полный mandatory-check registry.
- ORC-39: Raw candidate не публикуется как готовый. Expected: Нет UI-download/send пригодного задания; temp внутренний. Oracle: Publication marker и filesystem audit.

### B14

- SYS-06: Preview соответствует экспорту. Expected: Координаты, порядок, компонент и уже уложенный материал совпадают с replay. Oracle: Сопоставление ID и координат текстового G-code.
- SYS-07: Не принять all-planar за успех. Expected: Тест не проходит из-за отсутствия ожидаемой непланарной площади. Oracle: Маска положительного fixture и фактические роли дорожек.
- ORC-20: GUI export закрыт. Expected: Нет финального G-code, даже вне enabled кнопки. Oracle: Файловые артефакты и единый policy decision.
- ORC-21: CLI export закрыт. Expected: Ненулевой exit code; no final file. Oracle: Process exit + directory snapshot.

### B15

- PHY-01: Подтверждение профиля калибрами. Expected: Оформлен протокол измерений; мелкий торец измерен отдельно. Oracle: Измерения оператора, фото и привязка к hash.
- PHY-02: Планарная контрольная печать. Expected: Есть исходное качество/размеры для сравнения. Oracle: Протокол измерений и фото в согласованном освещении.
- PHY-03: Контролируемая первая крышка. Expected: Нет касаний/сдвига/видимых задиров; дефекты фиксируются по сегменту. Oracle: Реальные наблюдения; не симуляция.
- PRF-07: Неплоский стол при identity mesh contract. Expected: REJECT предусловия; не считать mesh OFF гарантией. Oracle: Независимая измерительная карта плоскости.

B07 actual body/cap continuation: B07-first-cap-material.md / ADR-0060 retain
exact completed body rows and selected cap material. Whole local continuous-run
support and local later original-surface feasibility pass; unsupported complete
ROI and future-cap queries refuse. This does not close complete fill or later
bead, motion/contact/order/export requirements.

Implementation order follows dependencies, starting with the shared input/job
boundary, followed by qualified upper analysis/body partition/material/planning,
then complete replay/gate/UI. Preserve useful positive fixtures and all negative
cases. Current progress is recorded in status.json and milestone evidence.
Do not mark Gate B complete while any mandatory requirement or B15 evidence
remains incomplete, indirect, missing or NOT_RUN.
