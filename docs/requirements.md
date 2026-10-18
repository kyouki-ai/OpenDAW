# Требования и границы

Цель MVP: Windows x64, устойчивый звук, встроенные инструменты, шаги, piano roll,
playlist, mixer, сохранение/открытие, offline WAV и основные undo/redo.
Полный паритет с FL Studio не обещается.

Реализована музыкальная основа: редакторы паттернов и клипов, synth/drums, фильтр,
выбор output device, loop/seek/click, file format v1, backup/recovery и PCM export.
Автоматические проверки: Debug/Release CTest, timing/model, deterministic render,
WAV header/PCM/peak, project round-trip и повреждённые файлы, GUI handlers/resize.

Оставшаяся аппаратная приёмка: прослушивание без щелчков, реальные drag/drop,
DPI 100/125/150/200%, смена мониторов, нагрузка и длительная работа устройства.
Открытие output и тишина при остановке проверяются отдельным device smoke.

До выпуска: выбрать совместимую лицензию, завершить приёмку и подготовить дистрибутив.
Текущий бренд OpenDAW — рабочий, проверка товарного знака не проводилась.
