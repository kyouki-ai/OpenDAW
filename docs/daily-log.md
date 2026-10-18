# Дневник

## 2026-10-18 — срез 01

Полностью прочитан исходный план. Изначально в папке был только пустой git repository.
Подготовлены native shell, preview transport, 3 themes, модели, timing conversion,
unit tests, CTest GUI smoke и Windows CI. CI не запускался в GitHub.
REA изучен по официальным источникам без установки. Созданы ADR, требования, лицензии и дизайн-система.

Среда: Windows x64, Visual Studio Build Tools 17.14, MSVC 19.44.35229,
Windows SDK 10.0.26100.0, CMake 3.31.6-msvc6, Node 24.19.0.
`py` launcher есть, установленного Python за ним нет; Python не требуется для сборки.
JUCE 8.0.12 скачан в игнорируемую .tools; SHA-256 закреплён в CMake.
Системные компоненты не устанавливались; компиляция ограничена одним worker и /MP1.

Проверки:

- Debug x64: сборка успешна, CTest core + gui_smoke — 2/2 PASS.
- Release x64: сборка успешна, CTest core + gui_smoke — 2/2 PASS.
- Core executable: 15 проверок musical timing, ошибок входа/переполнения, tempo boundaries,
  Play/Stop и сохранения ID при копировании — PASS (assertions не отключаются в Release).
- GUI: callbacks Play/Stop/BPM, Space/Escape, 3 темы, component bounds при 960×640/1280×800 — PASS.
- Просмотрены PNG рендера работающего приложения при обоих размерах, наложений не обнаружено.
- clang-format 19.1.5 --dry-run --Werror для собственных C++ sources — PASS.

Найден и исправлен реальный build blocker: juceaide rcfile успешно работает с относительным
ASCII argument, но падает на абсолютном пути с кириллицей. Windows resources заменены собственными
.rc/manifest, компилируемыми CMake/MSVC, без изменения исходников JUCE.
Повторная конфигурация также выявила сбой проверки версии juceaide. Финальный Windows-путь
переведён на JUCE_MODULES_ONLY: этот helper больше не собирается и не запускается.
При заключительной проверке обнаружен внутренний JUCE /MP: для ограничения его приоритета
CL заменён на _CL_=/MP1 (параметры добавляются после командной строки).
Первые сборки ограничивали MSBuild одним worker; внутренний /MP тогда мог переопределять CL.
Источник порядка параметров: https://learn.microsoft.com/en-us/cpp/build/reference/cl-environment-variables

Три концепта действительно сгенерированы встроенным image_gen и сохранены с фактическими prompts.
Владелец выбрал C / Ember / Sand: default theme, wave mark, amber Play и оформление shell применены.
Сайт с оригинальным дизайном, анимациями и переходами добавлен отдельным будущим этапом (website-brief.md).

Аудиоустройство, реальный звук, HiDPI мониторы и mouse-driven сценарии пока не проверены.
В следующем запросе владелец разрешил выгрузку в kyouki-ai/OpenDAW и ежедневные настоящие коммиты.
После уточнения выбрана публичная видимость для преподавателя. Создана активная ежедневная задача
Codex на 19:00 Asia/Yekaterinburg. Вход CLI подтверждён владельцем под kyouki-ai.
Создан публичный https://github.com/kyouki-ai/OpenDAW, подключён origin.
Первый локальный коммит: 21ea33b, ветка main; повторная проверка Release — 2/2 PASS.
Первый push отклонён GitHub из-за отсутствия scope workflow для `.github/workflows/windows.yml`.
Запрос дополнительного scope заблокирован автоматической проверкой: изменение доступа требует
отдельного явного согласия владельца. Затем владелец явно разрешил workflow и подтвердил
дополнительный доступ через официальный вход GitHub CLI.
Обычный push в main выполнен успешно; локальный и удалённый HEAD a9e9ce4 совпали.
Публичная история: https://github.com/kyouki-ai/OpenDAW/commits/main/.
GitHub Actions Windows build запущен; на момент первичной проверки статус queued,
успешное прохождение облачного CI пока не заявляется.
