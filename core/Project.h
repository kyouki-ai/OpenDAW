#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace opendaw
{
using Tick = std::int64_t;
inline constexpr Tick ppq = 960;

// UUID strings are generated at the application boundary, never in the audio callback.
struct EntityId
{
    std::string value;
    bool operator==(const EntityId &) const = default;
};
struct Note
{
    EntityId id;
    Tick start = 0;
    Tick duration = ppq;
    int pitch = 60;
    float velocity = 0.8f;
};
struct Pattern
{
    EntityId id;
    std::string name;
    Tick length = ppq * 4;
    std::vector<Note> notes;
};
struct Clip
{
    EntityId id;
    EntityId patternId;
    Tick start = 0;
    Tick length = ppq * 4;
};
struct Track
{
    EntityId id;
    std::string name;
    std::vector<Clip> clips;
    enum class Instrument
    {
        kick,
        snare,
        hat,
        sine,
        triangle
    } instrument = Instrument::sine;
    float gain = 0.75f;
    float pan = 0.0f;
    bool mute = false;
    bool solo = false;
    float effectGain = 1.0f;
    float filterCutoff = 20000.0f;
    bool filterEnabled = false;
};
struct Project
{
    EntityId id;
    std::string name = "Untitled session";
    double bpm = 124.0;
    int numerator = 4;
    int denominator = 4;
    int schemaVersion = 1;
    std::vector<Pattern> patterns;
    std::vector<Track> tracks;
    Tick loopStart = 0;
    Tick loopEnd = ppq * 16;
    bool loopEnabled = true;
    bool metronome = false;
    float masterGain = 0.8f;
};
[[nodiscard]] EntityId newId();
[[nodiscard]] bool validId(const EntityId &) noexcept;
[[nodiscard]] std::string validateProject(const Project &);
[[nodiscard]] Project makeDemoProject();
[[nodiscard]] bool validTempo(double bpm) noexcept;
// Throws on invalid musical/sample-rate inputs or int64 sample overflow.
[[nodiscard]] std::int64_t ticksToSamples(Tick ticks, double bpm, double sampleRate);
} // namespace opendaw
