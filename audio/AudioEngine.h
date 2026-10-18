#pragma once
#include "audio/SpscQueue.h"
#include "core/Project.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace opendaw
{
inline constexpr std::size_t maxChannels = 8, maxEvents = 8192, maxVoices = 64;
struct ScheduledEvent
{
    Tick tick = 0;
    std::uint32_t token = 0;
    std::uint8_t channel = 0, pitch = 60;
    bool noteOn = false;
    float velocity = 0;
};
struct RenderChannel
{
    Track::Instrument instrument = Track::Instrument::sine;
    float gain = 0.75f, pan = 0;
    bool mute = false, solo = false;
    float effectGain = 1, filterCutoff = 20000;
    bool filterEnabled = false;
};
struct RenderState
{
    std::array<ScheduledEvent, maxEvents> events{};
    std::array<RenderChannel, maxChannels> channels{};
    std::size_t eventCount = 0, channelCount = 0;
    std::uint64_t sequenceHash = 0;
    double bpm = 124;
    Tick loopStart = 0, loopEnd = ppq * 16;
    Tick songEnd = ppq * 16;
    float masterGain = 0.8f;
    bool loopEnabled = true, metronome = false;
};
// Compiles/validates on the message or worker thread, never in the callback.
[[nodiscard]] std::string compileProject(const Project &, RenderState &);
struct EngineMeters
{
    static_assert(std::atomic<float>::is_always_lock_free && std::atomic<std::int64_t>::is_always_lock_free);
    std::atomic<std::int64_t> positionSamples{0}, positionTicks{0};
    std::atomic<float> masterPeak{0}, cpuLoad{0};
    std::atomic<float> masterLeft{0}, masterRight{0};
    std::array<std::atomic<float>, maxChannels> channelPeak{};
    std::atomic<std::uint64_t> deadlineOverruns{0};
    std::atomic<std::uint64_t> callbackCount{0};
    std::atomic<bool> playing{false};
};
class AudioEngine
{
  public:
    AudioEngine();
    // Called only with the callback stopped. Does not touch UI-owned snapshot slots.
    void prepare(double sampleRate) noexcept;
    // Single producer: UI. False means defer/coalesce and retry on a later UI tick.
    bool publish(const RenderState &) noexcept;
    void play() noexcept
    {
        requestedPlaying_.store(true, std::memory_order_release);
    }
    void stop() noexcept
    {
        requestedPlaying_.store(false, std::memory_order_release);
        requestedSeek_.store(0, std::memory_order_release);
    }
    void seek(Tick tick) noexcept
    {
        requestedSeek_.store(tick < 0 ? 0 : tick, std::memory_order_release);
    }
    void process(float *const *outputs, int channels, int frames) noexcept;
    EngineMeters meters;

  private:
    struct Voice
    {
        bool active = false, releasing = false;
        std::uint32_t token = 0, noise = 1;
        std::uint8_t channel = 0;
        Track::Instrument type = Track::Instrument::sine;
        double phase = 0, frequency = 220, age = 0;
        float level = 0, velocity = 0;
    };
    void releaseAll() noexcept;
    void resetCursor() noexcept;
    void noteOn(const ScheduledEvent &) noexcept;
    float renderVoice(Voice &) noexcept;
    std::int64_t toSamples(Tick tick) const noexcept;
    std::array<RenderState, 3> states_{};
    std::array<bool, 3> free_{false, true, true}; // UI only
    SpscQueue<int, 4> ready_, retired_;
    std::atomic<bool> requestedPlaying_{false};
    std::atomic<Tick> requestedSeek_{-1};
    std::array<Voice, maxVoices> voices_{};
    std::array<float, maxChannels> gainL_{}, gainR_{};
    std::array<float, maxChannels> filterMemory_{}, filterAlpha_{}, effectGain_{};
    float smoothing_ = 0.004f;
    std::size_t cursor_ = 0;
    int active_ = 0;
    double sampleRate_ = 48000, samplesPerTick_ = 0;
    std::int64_t position_ = 0;
    std::uint32_t serial_ = 1;
    bool playing_ = false;
    float master_ = 0, click_ = 0;
    double clickPhase_ = 0;
};
} // namespace opendaw
