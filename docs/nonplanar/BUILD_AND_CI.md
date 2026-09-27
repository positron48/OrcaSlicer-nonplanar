# Сборка, CI и регрессия

## Статус

В этом комплекте нет готового CMake patch или доказанной сборки форка. Gate A должен произвести реальную сборку закреплённой Orca и сохранить команды/логи. Source audit подтвердил C++17 и существующий CMake-параметр `BUILD_TESTS`; tests используют Catch2/CTest (O08/O09). Успешный документационный checker этого не заменяет.

## Bootstrap

Проверить commit из lock и submodule SHAs; изучить скрипты сборки и workflow **на этой ревизии**, а не текущую страницу wiki для main. Сначала собрать неизменённый upstream. Сохранить compiler, SDK, CMake version, build options, dependency hashes и архитектуру.

Для macOS собирать нативный ARM64 target на Apple Silicon; смешанный universal/cross-build не использовать как доказательство выполнения тестов ARM64. В выбранном CMake cross-compilation может отключать BUILD_TESTS; явно проверить effective CMakeCache (O08).

После штатной настройки зависимостей добавить `-DBUILD_TESTS=ON` к конфигурации CMake. Команда без dependency prefix/toolchain не является самодостаточным рецептом. Gate A выпускает проверенные `build-macos.md`, `build-linux.md` и CI в самом форке.

В существующем build directory:

```sh
ctest --test-dir <actual-build-directory> -N
ctest --test-dir <actual-build-directory> --output-on-failure
```

`<actual-build-directory>` заменить реальным путём созданной сборки. Эти команды — протокол запуска тестов, а не утверждение об их текущем наличии. Пустая выборка/0 tests found = FAIL gate, не зелёная приёмка.

## CI-наборы

| Набор | Когда | Что проверяет |
|---|---|---|
| Contract/schema/unit | Каждый PR | Координаты, объём, профили, состояние, tiny analytical cases |
| Geometry/negative | Каждый safety PR | CCD, finite tip, time-material, поддержка/контакт |
| Stock differential | Каждый shared C++/config patch | Нативные OFF и ZAA против исходного commit |
| Hybrid integration | Каждый merge в feature branch | Импорт → тело/крышка → конечный verifier |
| Export/3MF/UI state | Каждый integration/UI patch | Нет обходов, stale callback, round-trip, cancellation |
| Property/fuzz/mutation | Расширенный CI | Фиксируемые seed, memory safety, отрицательные вариации |
| Clean platform build | Release candidate | macOS ARM64 и Linux x86_64, непустые тесты |
| Benchmark | Gate A и candidate | Время, memory, число треугольников/движений |
| Physical | Только оператор | Профиль, пробная печать, повторяемость |

## Golden baselines

До функциональных изменений сохранить входные 3MF/STL и точные resolved presets; stock OFF/ZAA G-code и семантический replay. Игнорировать только ограниченные поля вроде timestamp/version string, перечисленные в normalize policy. Не нормализовать E/F/XYZ, число движений, порядок, support mask или warnings, от которых зависит поведение.

Когда новое поведение необходимо в hybrid, OFF остаётся прежним. Golden обновляется отдельно с обоснованием и diff. Для межплатформенной геометрии допускаются только документированные численные допуски; алгоритмическая недетерминированность не прячется под большим epsilon.

## Инструменты качества

Использовать уже закреплённые upstream test libraries. Sanitizers/fuzzers добавлять к поддерживаемым native targets; если конкретная конфигурация недоступна, сохранять NOT_RUN с причиной. Ошибка библиотеки, утечка процесса после cancel и неконтролируемая память — дефект, даже когда G-code не экспортирован.

Новые зависимости не должны молча скачиваться во время слайсинга. Build cache ключи включают upstream SHA, compiler/SDK, dependency versions и flags. Offline runtime — обязательный сценарий.

## Изоляция и упаковка

Отдельные executable/app/bundle identity, config/cache directories и build channel. Не регистрировать experimental build как обязательную замену штатной Orca и не переключать её auto-update на stock. Проверить установку рядом с существующей Orca, импорт preset как копии и полное отсутствие записи в чужой config.

Дистрибутив содержит source revision, заметное предупреждение об экспериментальном режиме, licenses/notices и совместимые схемы. API-ключи, аккаунты пользователя и рабочие конфиги в artifact/лог не попадают.
