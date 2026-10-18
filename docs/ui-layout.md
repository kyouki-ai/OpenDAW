# Компоновка

Нативная рамка Windows, content 1280×800, минимум 960×640. Header 88 logical px:
бренд, Play/Stop, BPM, позиция из sample clock, размер и selector темы.
Слева панель с файлами, undo/redo, loop/click, инструментом и Audio settings.
По центру arrangement с клипами, mute/solo и канальными уровнями.
Снизу вкладки step sequencer и piano roll; справа master fader и meters.
F3 открывает mixer на месте центральных редакторов: полосы каналов, pan, low-pass,
input gain, volume и mute/solo. При небольшом окне используется viewport с прокруткой.

Step sequencer: click toggles, wheel меняет velocity; длина 16/32 шага в заголовке.
Piano roll: click добавляет, right-click удаляет, drag перемещает, край меняет длину;
Shift выделяет несколько нот, Ctrl+A/C/V/D, Delete и velocity stems.
Стрелки перемещают выделение; Shift+Left/Right меняет длину, Shift+Up/Down — октаву.
F2 или двойной щелчок по имени сессии позволяет её переименовать с undo/redo.
Grid в заголовке переключает 1/8, 1/16 и 1/32. Ctrl-wheel меняет масштаб,
Shift-wheel прокручивает время; обычное колесо в piano roll — высоту нот.
Arrangement: click линейки — seek; Shift-drag — loop; drag клипа — перенос/край — длина;
Shift-click — копия, right-click — удалить. Колесо над дорожкой — gain, Shift-wheel — pan.
Ctrl-wheel над arrangement — zoom, колесо над клипами — горизонтальная прокрутка.
Клип можно перенести на другую дорожку, сохранив ссылку на паттерн.

Все ноты, клипы и meters отражают текущую модель или реальную обработку.
Подсказки показывают доступные действия. Цвета определены общими ThemeTokens.
Строка состояния сообщает устройство, формат, экспорт или ошибку.
GUI smoke проверяет handlers и границы components при 960×640 и 1280×800;
сохраняет реальные рендеры работающего приложения. Это не аппаратные DPI screenshots.

Дальнейшая работа: визуальная полировка,
клавиатурная навигация и тестирование на физических DPI/мониторах.
