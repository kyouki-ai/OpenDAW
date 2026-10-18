# План развития

Последовательные этапы первого музыкального MVP. Состояние относится к прототипу;
непроверенные аппаратные сценарии отдельно отмечены.

| Этапы | Результат | Состояние |
|---|---|---|
| 01–07 | CMake/JUCE, UUID, model, timing, CI | реализовано; локальные unit/smoke проходят |
| 08–12 | output device, silence callback, sample clock, loop/click | реализовано; устройство и тишина проверены, прослушивание впереди |
| 13–20 | scheduler, synth, envelopes, gain | реализована основа; настройки приложения ещё не сохраняются |
| 21–28 | step sequencer | редактирование, velocity, 16/32 steps и playback работают |
| 29–38 | piano roll | add/delete/move/resize, Shift-selection, duplicate, velocity; zoom/scroll и paste требуют доработки |
| 39–46 | arrangement | clips, move/resize/duplicate, seek, loop; zoom и перенос между дорожками впереди |
| 47–54 | mixer | gain/pan/mute/solo, meters и master; отдельные strips и filter впереди |
| 55–60 | persistence | JSON v1, validation, atomic save, backup; autosave/recovery впереди |
| 61–65 | WAV, equivalence, undo/redo | реализовано; PCM/отмена/round-trip покрыты проверками |
| 66–70 | hotkeys, DPI/performance, end-to-end | основные hotkeys есть; реальные мышь/HiDPI/нагрузка ещё не проверены |

Ближайшие задачи: paste без подмены clipboard, масштабирование редакторов, mixer strips,
filter, настройки приложения и recovery. Затем полноценный сценарий создания композиции,
визуальная полировка и Release-проверки.

Оформление: Ember / Sand, собственная векторная отрисовка компонентов.
VST3, запись, sends/PDC и сложная автоматизация — следующие версии.
После устойчивой рабочей версии — сайт с анимациями и реальными демонстрациями.
Лицензия для выпуска пока не выбрана.
