# Аудит перед экспериментальным релизом

Проверь RELEASE_CHECKLIST, requirements.json, backlog.json и acceptance_tests.csv против фактического кода, build/test artifacts и physical run records. Все исходные состояния комплекта запланированы; никакого унаследованного PASS нет.

Проверь обе целевые платформы, nonempty test execution, все обязательные negative/positive tests, native baselines, final-byte verifier, экспортные каналы, 3MF, изоляцию и hashes. Физические результаты должны быть операторскими записями, не рендерами.

Составь перечень выполненных требований с evidence и отдельный список NOT_IMPLEMENTED / NOT_RUN / BLOCKED. Не объявляй релиз готовым при отсутствующей физической квалификации или безопасности. Не меняй код/пороги только ради прохождения аудита.
