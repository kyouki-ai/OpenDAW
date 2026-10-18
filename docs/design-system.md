# Дизайн-система 0.1

Основное направление — **Ember / Sand**: ясная иерархия, выразительные пропорции,
сдержанный свет и собственные контролы. Ember — тема по умолчанию.
Приняты волновой знак, круглая янтарная Play, тёплая палитра и будущая композиция панелей концепта C.

| Token | A Graphite / Mint | B Ultraviolet / Ice | C Ember / Sand |
|---|---|---|---|
| background | #101419 | #11101B | #181412 |
| surface | #181E25 | #1B182B | #231E1A |
| raised | #222B34 | #29243D | #302923 |
| border | #303C48 | #3A3352 | #493C31 |
| text | #E8EDF2 | #F2EFFF | #F4EEE7 |
| muted | #A5B1BF | #B6ACCD | #BFB0A2 |
| accent | #6FE3C0 | #BDA2FF | #FFBD7A |
| secondary | #BBA3FF | #7EDCE8 | #94C9B7 |

Реализация: ui/Theme.*; цвет UI не хранится в audio model. Один LookAndFeel живёт дольше дочерних controls,
снимается до удаления. Выбранная палитра сохраняется с настройками приложения при закрытии.

Типографика: системный sans, ориентир Segoe UI; 23–24 заголовки, 17–20 секции, 13–14 основной текст,
10–11 вторичные подписи. В будущей плотной рабочей области минимум 12 для actionable labels.
Сетка отступов: 4/8/12/16/24/32 logical px. Радиусы 7 controls / 10 panels / 16 empty state.
Контролы 36 px высотой, minimum focus border 2 px. Иконки собственные paths, stroke 1.5–2 logical px.
Не обозначать выбранное состояние одним цветом: меняются fill, label и состояние toggle.

Button states: default raised; hover +12% brightness; pressed darker; selected accent и тёмный текст;
disabled alpha 40%; focused outline 2 px accent. Tab/Shift+Tab, Enter/Space для кнопок.
Глобальный Space переключает воспроизведение, Escape останавливает; в текстовом вводе сохраняется
обычное поведение редактирования. Playhead и meters обновляются на UI Timer 30 Hz;
в покое музыкальные панели не перерисовываются непрерывно.

Contrast target: 4.5:1 основной текст, 3:1 крупный текст/границы важных controls.
Фактический WCAG-аудит и screen reader verification ещё не выполнены.
Требование DPI: 100/125/150/200%, JUCE logical coordinates. Screenshot масштаба сам по себе
не подтверждает смену physical monitor DPI. Check list: labels, фокус, popup, minima, no overlaps.
