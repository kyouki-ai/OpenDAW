# Архитектура OpenDAW

Статус: срез 01 реализует core, preview Transport и оболочку. Остальное ниже — проектирование,
не утверждение о готовом аудиодвижке. Стек: C++20/CMake/JUCE 8.0.12; GUI отдельно от DSP.

## Модель и время

`core/Project.h`: Project { UUID, schemaVersion=1, name, bpm, numerator, denominator, patterns, tracks }.
Pattern { UUID, lengthTicks, notes }; Note { UUID, start, duration, MIDI pitch, velocity };
Track { UUID, name, clips }; Clip { UUID, patternId, start, length }.
Сейчас EntityId хранит строку; генерация и проверка UUID на границе приложения/загрузки — следующий срез.
Сериализации и полной проверки ссылочной целостности пока нет.

960 PPQ, signed int64 Tick, quarter-note based, nonnegative project positions.
`ticksToSamples(ticks, bpm, sampleRate)` округляет абсолютную позицию, отклоняет невалидный темп,
sample rate, отрицательное время и переполнение. Не накапливать округление шаг за шагом.
Размер такта влияет на grid: ticksPerBar = PPQ * numerator * 4 / denominator.
Tempo map позднее; для первого playback tempo неизменен в пределах обработанного блока.

## Конкретные интерфейсы

Реализованный `Transport`: setTempo(double)->bool, play(), stop(), isPlaying(), tempo(), position().
Это объект message thread; его нельзя передавать callback. При интеграции engine UI будет отправлять команды,
а позиция станет публикуемым snapshot. Не использовать GUI Timer как аудиочасы.

Предлагаемые контракты следующего среза (сейчас не реализованы):

```cpp
struct RenderContext { double sampleRate; int frames; int64_t absoluteSample; };
struct NoteEvent { int frameOffset; uint8_t pitch; float velocity; bool noteOn; };
class Instrument {
public:
    virtual ~Instrument() = default; // destruction outside callback
    virtual void prepare(double sampleRate, int maxFrames) = 0;
    virtual void reset() noexcept = 0;
    virtual void process(const RenderContext&, std::span<const NoteEvent>,
                         std::span<float> left, std::span<float> right) noexcept = 0;
};
class AudioEngine {
public:
    void prepare(double sampleRate, int maxFrames); // device stopped
    bool submit(const TransportCommand&) noexcept; // bounded SPSC; UI producer
    bool publishPrepared(StateSlotId) noexcept;    // worker-ready state
    void process(float* const* output, int channels, int frames) noexcept;
    EngineMeters readMeters() const noexcept;     // UI consumer
    void stopAndRelease();                         // joins outside callback
};
```

Все выходные каналы заполняются; необрабатываемые — нулями. Events отсортированы по sample,
затем note-off перед note-on, затем стабильный ID. Блок — [start, end), событие на end относится
к следующему блоку. При seek/stop/loop сбросить активные голоса по определённой политике.

## Потоки и владение

Message thread владеет изменяемым Project, undo-командами и компонентами. Worker готовит immutable
RenderState (scheduler, prepared instruments, routing, samples) и читает/пишет диск.
Callback владеет текущим playhead, DSP state, заранее выделенными буферами и голосами.

До старта устройства создать пул минимум из трёх state slots. Worker заполняет только свободный slot,
публикует индекс через bounded SPSC с release/acquire. Callback принимает индекс на границе блока,
возвращает предыдущий индекс через retirement queue. Только worker освобождает память retired slot.
Очереди ограничены, при заполнении worker откладывает публикацию, UI coalesces tempo/gain; stop получает
отдельный atomic request. Callback никогда не ждёт свободного slot. Не использовать atomic shared_ptr:
последний decrement может вызвать удаление в audio thread.

UI → audio: POD-команды через SPSC. Audio → UI: атомарные счётчики/уровни с фиксированным размером.
Проверить lock-free свойства используемых atomics на x64. CPU load измерять вокруг process,
не писать логи внутри него. Separate counters: callback deadline overruns и backend-reported xruns;
не называть любой длинный callback аппаратным dropout.

Offline engine использует тот же scheduler и DSP с фиксированным seed и без GUI/device.
Загрузка assets, конструирование графа и подготовка инструментов выполняются заранее.

## Формат проекта (предложение)

Папка `.opendaw` с `project.json`, `assets/`, относительными нормализованными путями и UUID.
Валидация версии, bounds, уникальности ID и ссылок перед заменой текущего проекта.
Не загружать `../` или абсолютные asset paths. Missing assets — явный отчёт, не тихая подмена.
Сохранение: sibling temporary file → flush → backup → atomic replacement на том же томе;
Windows API и recovery будут выбраны с отдельными тестами ошибок. Нынешний код ничего не сохраняет.

## Модули

app — lifecycle; core — model/validation/undo; audio — clock/device/engine;
midi — event editing; sequencer — steps/clips/scheduler; instruments — synth/sampler;
mixer — buses/meters; effects — DSP; ui — JUCE components/theme;
persistence — schema/recovery; export — offline WAV; tests — deterministic verification.
Не создавать пустые реализации будущих функций ради заполнения папок.
