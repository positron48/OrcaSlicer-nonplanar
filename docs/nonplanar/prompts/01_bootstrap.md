# Первое задание Codex

Работаем в репозитории форка OrcaSlicer. Прочитай `docs/nonplanar/README.md`, `SPEC.md`, `AGENTS.md`, `ORCA_INTEGRATION.md`, `DATA_CONTRACTS.md`, `BUILD_AND_CI.md`, `BACKLOG.md` и ADR. Если комплект размещён иначе, сначала найди его каталог и не дублируй файлы.

Выполни A01–A03; затем подготовь конкретные небольшие задачи A04–A08. База строго из orca.lock.json. Проверь локальный git, dependency/submodule ревизии и реальную сборку stock Orca на доступной целевой платформе. Сохрани точные команды и effective BUILD_TESTS; CTest должен обнаружить и запустить непустой набор. Вторую недоступную платформу пометь NOT_RUN, не делай вид, что она проверена.

Найди фактические точки вызова native ZAA, создание body surfaces/perimeters/solid fill, G-code transforms и все file/CLI/3MF/upload routes. Сохрани карту вызовов с файлами/символами на выбранном commit. Получи baseline OFF/ZAA на маленьком клине и куполе, без отправки принтеру. Не объявляй эти baseline проверенными нашим будущим verifier.

Зафиксируй typed IR: absolute physical XYZ, явный объём, retraction state, frames, numeric budget, separate D_nominal/upper/lower. Новый код не должен передавать absolute Z в существующее поле relative ZAA offset. Предложи минимальное подключение к planar engine Orca; внешний PrusaSlicer/Python-server/новый UI не создавать.

Начни с небольшого проверяемого патча и независимых tests. Не реализуй всё ТЗ одним коммитом. Не изменяй presets пользователя, cloud, firmware, не запускай принтер. В отчёте различай: просмотрено, собрано, тесты выполнены, только спроектировано, NOT_RUN; перечисли блокеры и наблюдаемые результаты.
