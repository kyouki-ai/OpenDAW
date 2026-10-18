# План развития

Последовательные этапы первого музыкального MVP. Состояние относится к прототипу;
непроверенные аппаратные сценарии отдельно отмечены.

| Этапы | Результат | Состояние |
|---|---|---|
| 01–07 | CMake/JUCE, UUID, model, timing, CI | реализовано; локальные unit/smoke проходят |
| 08–12 | output device, silence callback, sample clock, loop/click | реализовано; устройство и тишина проверены, прослушивание впереди |
| 13–20 | scheduler, synth, envelopes, gain | synth, scheduler и настройки приложения реализованы |
| 21–28 | step sequencer | редактирование, velocity, 16/32 steps и playback работают |
| 29–38 | piano roll | add/delete/move/resize, Shift-selection, duplicate, velocity; paste, grid 1/8–1/32, quantize, zoom/scroll реализованы |
| 39–46 | arrangement | clips, move/resize/duplicate, seek, loop; zoom/scroll и перенос между дорожками реализованы |
| 47–54 | mixer | gain/pan/mute/solo, meters и master; strips, input gain и low-pass реализованы |
| 55–60 | persistence | JSON v1, validation, atomic save, backup; контрольные копии и recovery с fallback на backup реализованы |
| 61–65 | WAV, equivalence, undo/redo | реализовано; PCM/отмена/round-trip покрыты проверками |
| 66–70 | hotkeys, DPI/performance, end-to-end | основные hotkeys есть; реальные мышь/HiDPI/нагрузка ещё не проверены |

Ближайшие задачи: полноценный сценарий создания композиции, параметры огибающих,
улучшение навигации и доступности, визуальная полировка и аппаратная проверка.

Оформление: Ember / Sand, собственная векторная отрисовка компонентов.
VST3, запись, sends/PDC и сложная автоматизация — следующие версии.
Сайт с интерактивным ритмом и реальными демонстрациями опубликован:
https://kyouki-ai.github.io/OpenDAW/.
Лицензия для выпуска пока не выбрана.
