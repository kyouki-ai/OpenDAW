# План

Источник: master-plan.md. Номера дней — этапы; не заявлять их как реально прожитые дни.

| Этапы | Результат | Состояние |
|---|---|---|
| 01–04 | структура, CMake/JUCE, Debug/Release, unit/smoke/CI | реализация подготовлена; фактические проверки — в daily-log |
| 05–07 | UUID validation, модель проекта, timing, ADR | базовые структуры и конверсия есть; полная валидация впереди |
| 08–12 | устройство, silence callback, transport sample clock, metronome, loop | следующий вертикальный срез |
| 13–20 | events, scheduler, sine, ADSR, gain, тест render | запланировано |
| 21–28 | редактируемый step sequencer с реальным звуком | запланировано |
| 29–38 | piano roll: ноты, выделение, snap, velocity | запланировано |
| 39–46 | playlist: clips/tracks/seek/loop | запланировано |
| 47–54 | mixer, gain/pan/meters/filter/routing | запланировано |
| 55–60 | schema, атомарное сохранение, recovery | запланировано |
| 61–65 | offline WAV, online equivalence, undo/redo | запланировано |
| 66–70 | hotkeys, DPI/performance, Release и end-to-end | запланировано |

Визуальная работа: три концепта готовы; владелец выбрал C / Ember / Sand → shared controls → editor panels.
Критерий следующего среза: выбранное output-устройство, гарантированная тишина callback,
sample clock без UI Timer, lifecycle-тесты и честный отчёт о физическом звуке.

VST3, запись, PDC и сложная автоматизация — позже устойчивого MVP.
После рабочей версии — авторский сайт приложения с анимациями и переходами;
требования владельца зафиксированы в website-brief.md. Сейчас не опубликован.
Лицензия, GitHub и точные правила учебного задания остаются открытыми решениями.
