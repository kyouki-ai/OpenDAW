# Исследование

## 2026-10-18 — публичные источники и независимое решение

Цель: определить минимальный нативный стек, musical timing и безопасную границу UI/audio.
Основание: чтение публичной документации и лицензий. Декомпиляция и анализ закрытых приложений не проводились.

| Источник | Прочитано / наблюдение | Собственное решение |
|---|---|---|
| [JUCE CMake](https://github.com/juce-framework/JUCE/blob/8.0.12/CMakeLists.txt) | версия, минимум CMake 3.22, helper tools | закрепить 8.0.12; single-worker build включая juceaide |
| [JUCE callback](https://docs.juce.com/master/classjuce_1_1AudioIODeviceCallback.html) | device lifecycle и output callback contract | будущий adapter отдельно от моделей и UI |
| [LMMS README](https://github.com/LMMS/lmms) | документированы pattern editor, piano roll, song editor, mixer | отдельные модели Pattern и Clip; не копировать код/ресурсы |
| [Ardour](https://github.com/Ardour/ardour) | публичная структура проекта и лицензия репозитория | учитывать отдельные подсистемы; внутренний DSP в этом срезе не исследован |
| [Zrythm](https://github.com/zrythm/zrythm) | публичное описание DAW и дерево репозитория | самостоятельная композиция панелей; код не заимствован |
| [REA](https://github.com/morluto/rea) | установка и ограничения, см. отдельную оценку | сейчас не нужен |

PPQ 960, test vectors, bounded state ownership и палитры — наши инженерные решения.
Открытые DAW здесь не устанавливались и не запускались: это обзор источников, не сравнительное тестирование поведения.
Для дальнейших исследований фиксировать конкретный файл/версию, разрешение и результаты, не переносить чужой код.
