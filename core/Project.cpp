#include "core/Project.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>
#include <unordered_set>

namespace opendaw
{
EntityId newId()
{
    static thread_local std::mt19937 generator(std::random_device{}());
    std::array<unsigned char, 16> bytes{};
    for (auto &byte : bytes)
        byte = static_cast<unsigned char>(generator() & 255u);
    bytes[6] = static_cast<unsigned char>((bytes[6] & 15u) | 64u);
    bytes[8] = static_cast<unsigned char>((bytes[8] & 63u) | 128u);
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(36);
    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            result += '-';
        result += hex[bytes[i] >> 4u];
        result += hex[bytes[i] & 15u];
    }
    return {result};
}
bool validId(const EntityId &id) noexcept
{
    if (id.value.size() != 36)
        return false;
    for (std::size_t i = 0; i < 36; ++i)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            if (id.value[i] != '-')
                return false;
        }
        else if (!((id.value[i] >= '0' && id.value[i] <= '9') || (id.value[i] >= 'a' && id.value[i] <= 'f')))
            return false;
    }
    return id.value != "00000000-0000-0000-0000-000000000000";
}
std::string validateProject(const Project &p)
{
    constexpr Tick limit = ppq * 4 * 4096;
    if (p.schemaVersion != 1 || !validTempo(p.bpm) || p.numerator < 1 || p.numerator > 16 ||
        (p.denominator != 1 && p.denominator != 2 && p.denominator != 4 && p.denominator != 8 &&
         p.denominator != 16))
        return "Unsupported project version or musical clock";
    if (p.name.size() > 256 || p.tracks.size() > 8 || p.patterns.size() > 64 || p.loopStart < 0 ||
        p.loopEnd <= p.loopStart || p.loopEnd > limit || !std::isfinite(p.masterGain) || p.masterGain < 0 ||
        p.masterGain > 1)
        return "Project exceeds supported bounds";
    std::unordered_set<std::string> ids, patterns;
    const auto insert = [&ids](const EntityId &id) { return validId(id) && ids.insert(id.value).second; };
    if (!insert(p.id))
        return "Invalid project UUID";
    for (const auto &pattern : p.patterns)
    {
        if (!insert(pattern.id) || pattern.name.size() > 256 || pattern.length < ppq / 4 ||
            pattern.length > ppq * 16 || pattern.length % (ppq / 4) != 0 || pattern.notes.size() > 512)
            return "Invalid pattern or duplicate UUID";
        patterns.insert(pattern.id.value);
        for (const auto &n : pattern.notes)
            if (!insert(n.id) || n.start < 0 || n.start >= pattern.length || n.duration <= 0 ||
                n.duration > pattern.length - n.start || n.pitch < 0 || n.pitch > 127 ||
                !std::isfinite(n.velocity) || n.velocity < 0 || n.velocity > 1)
                return "Invalid note or duplicate UUID";
    }
    for (const auto &track : p.tracks)
    {
        if (!insert(track.id) || track.name.size() > 256 || track.clips.size() > 256 ||
            !std::isfinite(track.gain) || track.gain < 0 || track.gain > 1 || !std::isfinite(track.pan) ||
            track.pan < -1 || track.pan > 1 || static_cast<int>(track.instrument) < 0 ||
            static_cast<int>(track.instrument) > 4 || !std::isfinite(track.effectGain) ||
            track.effectGain < 0 || track.effectGain > 2 || !std::isfinite(track.filterCutoff) ||
            track.filterCutoff < 20 || track.filterCutoff > 20000 ||
            (!track.patternId.value.empty() && !patterns.contains(track.patternId.value)))
            return "Invalid track or duplicate UUID";
        for (const auto &clip : track.clips)
            if (!insert(clip.id) || !patterns.contains(clip.patternId.value) || clip.start < 0 ||
                clip.length <= 0 || clip.start > limit || clip.length > limit - clip.start)
                return "Invalid clip or missing pattern reference";
    }
    return {};
}
Project makeDemoProject()
{
    Project p;
    p.id = newId();
    p.name = "After hours";
    const std::array<const char *, 5> names{"Kick", "Snare", "Closed hat", "Sub bass", "Velvet keys"};
    const std::array<Track::Instrument, 5> types{Track::Instrument::kick, Track::Instrument::snare,
                                                 Track::Instrument::hat, Track::Instrument::triangle,
                                                 Track::Instrument::sine};
    for (std::size_t channel = 0; channel < names.size(); ++channel)
    {
        Pattern pattern{newId(), names[channel]};
        if (channel == 4)
        {
            pattern.length = ppq * 8;
            constexpr int chords[4][3]{{57, 60, 64}, {53, 57, 60}, {60, 64, 67}, {55, 59, 62}};
            for (int chord = 0; chord < 4; ++chord)
                for (int pitch : chords[chord])
                    pattern.notes.push_back({newId(), chord * ppq * 2, ppq * 2 - ppq / 8, pitch, 0.55f});
        }
        for (int step = 0; step < 16; ++step)
        {
            bool active = channel == 0   ? step % 4 == 0
                          : channel == 1 ? step % 8 == 4
                          : channel == 2 ? step % 2 == 0
                                         : step == 0 || step == 6 || step == 10 || step == 14;
            if (active && channel < 4)
            {
                const int pitch = channel == 3 ? (step < 8 ? 45 : step < 14 ? 48 : 43) : 60;
                pattern.notes.push_back({newId(), step * ppq / 4, channel == 3 ? ppq / 2 : ppq / 8, pitch,
                                         channel == 2 ? (step % 4 == 0 ? 0.62f : 0.38f) : 0.8f});
            }
        }
        Track track{newId(), names[channel]};
        track.patternId = pattern.id;
        track.instrument = types[channel];
        track.gain = channel == 2 ? 0.6f : channel == 4 ? 0.5f : 0.75f;
        track.clips.push_back({newId(), pattern.id, 0, p.loopEnd});
        p.patterns.push_back(std::move(pattern));
        p.tracks.push_back(std::move(track));
    }
    return p;
}
bool validTempo(double bpm) noexcept
{
    return std::isfinite(bpm) && bpm >= 20.0 && bpm <= 300.0;
}

std::int64_t ticksToSamples(Tick ticks, double bpm, double sampleRate)
{
    if (!validTempo(bpm) || !std::isfinite(sampleRate) || sampleRate <= 0.0 || ticks < 0)
        throw std::invalid_argument("Invalid clock parameters");
    const auto samples = static_cast<long double>(ticks) * 60.0L * sampleRate / (bpm * ppq);
    if (samples >= static_cast<long double>(std::numeric_limits<std::int64_t>::max()) - 0.5L)
        throw std::overflow_error("Sample position overflow");
    return static_cast<std::int64_t>(std::round(samples));
}
} // namespace opendaw
