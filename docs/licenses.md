# Лицензии и происхождение

Проверено 2026-10-18. Это инженерная инвентаризация; окончательный способ распространения не выбран.
У собственного кода нет произвольно назначенной LICENSE. Локальная сборка не означает готовность публичного релиза.

| Компонент | Версия / источник | Условия и действие |
|---|---|---|
| Собственный C++/docs | срез 01 | решение владельца по лицензии до распространения |
| JUCE | 8.0.12, официальный archive, SHA в CMake | AGPLv3 или коммерческая лицензия; бесплатность программы не отменяет условия |
| libpng / zlib | поставляются с выбранным JUCE | zlib-style / zlib; сохранить notices при упаковке |
| JPEG | поставляется с JUCE graphics | Independent JPEG Group; сохранить notices |
| HarfBuzz | поставляется с JUCE graphics | Old MIT, проверить included COPYING перед release |
| SheenBidi | поставляется с JUCE graphics | Apache-2.0, сохранить included LICENSE |
| VST3 SDK | не подключён | текущий официальный SDK MIT; версия JUCE содержит свои файлы, проверять именно их |
| REA | не установлен | собственный код MIT; providers отдельно |
| Segoe UI / системный шрифт | установлен Windows, не поставляется с проектом | используется системный fallback; файлы шрифта не копируются |
| Иконки | собственная vector drawing | сторонние packs не используются |
| Концепты | встроенный image_gen | сгенерированные предложения; не внедрять как bitmap интерфейс |
| CMake / MSVC / Windows SDK | имеющиеся build tools | не поставляются в runtime; применяются их условия разработки |

Используются juce_gui_basics, juce_audio_devices и их core/events/data_structures/graphics/audio_basics dependencies.
WAV PCM записывается собственным кодом без внешних codecs.
Audio formats, plugin hosting, WebView, curl и ASIO сейчас не подключены.
Перед выпуском собрать точные notices из закреплённого архива и проверить все включённые codecs/platform paths.
Не переносить общую лицензию репозитория на каждый вложенный компонент.

Пути выбора: совместимое открытое распространение с выполнением AGPL либо подходящий JUCE commercial plan.
Выбор лицензии и eligibility коммерческого плана остаются за владельцем; никаких покупок/аккаунтов не оформлялось.

Источники: [JUCE 8.0.12 LICENSE](https://github.com/juce-framework/JUCE/blob/8.0.12/LICENSE.md),
[JUCE EULA](https://juce.com/legal/juce-8-licence/),
[VST3 LICENSE](https://github.com/steinbergmedia/vst3sdk/blob/master/LICENSE.txt).
Исходники других DAW в сборку не входят.
