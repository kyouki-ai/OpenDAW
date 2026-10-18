#include "audio/AudioEngine.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>

namespace opendaw
{
std::string compileProject(const Project &project, RenderState &state)
{
    if (const auto error = validateProject(project); !error.empty())
        return error;
    state = RenderState{};
    state.bpm = project.bpm;
    state.loopStart = project.loopStart;
    state.loopEnd = project.loopEnd;
    state.loopEnabled = project.loopEnabled;
    state.songEnd = ppq * project.numerator * 4 / project.denominator;
    for (const auto &track : project.tracks)
        for (const auto &clip : track.clips)
            state.songEnd = std::max(state.songEnd, clip.start + clip.length);
    state.metronome = project.metronome;
    state.masterGain = project.masterGain;
    state.channelCount = project.tracks.size();
    std::uint32_t token = 0;
    for (std::size_t c = 0; c < project.tracks.size(); ++c)
    {
        const auto &track = project.tracks[c];
        state.channels[c] = {track.instrument, track.gain, track.pan, track.mute, track.solo};
        state.channels[c].effectGain = track.effectGain;
        state.channels[c].filterCutoff = track.filterCutoff;
        state.channels[c].filterEnabled = track.filterEnabled;
        for (const auto &clip : track.clips)
        {
            const auto pattern = std::find_if(project.patterns.begin(), project.patterns.end(),
                                              [&clip](const auto &p) { return p.id == clip.patternId; });
            if (pattern->notes.empty())
                continue;
            for (Tick repeat = 0; repeat < clip.length; repeat += pattern->length)
            {
                for (const auto &note : pattern->notes)
                {
                    const Tick start = clip.start + repeat + note.start;
                    const Tick end = std::min(start + note.duration, clip.start + clip.length);
                    if (start >= clip.start + clip.length || note.velocity == 0)
                        continue;
                    if (state.eventCount + 2 > maxEvents)
                        return "Arrangement exceeds 4096 scheduled notes";
                    ++token;
                    state.events[state.eventCount++] = {start,
                                                        token,
                                                        static_cast<std::uint8_t>(c),
                                                        static_cast<std::uint8_t>(note.pitch),
                                                        true,
                                                        note.velocity};
                    state.events[state.eventCount++] = {
                        end,   token, static_cast<std::uint8_t>(c), static_cast<std::uint8_t>(note.pitch),
                        false, 0};
                }
            }
        }
    }
    std::sort(state.events.begin(), state.events.begin() + static_cast<std::ptrdiff_t>(state.eventCount),
              [](const auto &a, const auto &b)
              {
                  if (a.tick != b.tick)
                      return a.tick < b.tick;
                  if (a.noteOn != b.noteOn)
                      return !a.noteOn;
                  return a.token < b.token;
              });
    std::uint64_t hash = 1469598103934665603ull;
    const auto add = [&hash](std::uint64_t value) { hash = (hash ^ value) * 1099511628211ull; };
    for (const auto &channel : state.channels)
        add(static_cast<std::uint64_t>(channel.instrument));
    for (std::size_t i = 0; i < state.eventCount; ++i)
    {
        const auto &event = state.events[i];
        add(static_cast<std::uint64_t>(event.tick));
        add(event.token);
        add(event.channel);
        add(event.pitch);
        add(event.noteOn);
        add(std::bit_cast<std::uint32_t>(event.velocity));
    }
    state.sequenceHash = hash;
    return {};
}
AudioEngine::AudioEngine()
{
    prepare(48000);
}
void AudioEngine::prepare(double sampleRate) noexcept
{
    const Tick preserved = requestedPlaying_.load() && samplesPerTick_ > 0
                               ? static_cast<Tick>(static_cast<double>(position_) / samplesPerTick_)
                               : 0;
    sampleRate_ =
        std::isfinite(sampleRate) && sampleRate >= 8000 && sampleRate <= 768000 ? sampleRate : 48000;
    samplesPerTick_ = sampleRate_ * 60.0 / (states_[active_].bpm * static_cast<double>(ppq));
    voices_.fill({});
    gainL_.fill(0);
    gainR_.fill(0);
    filterMemory_.fill(0);
    filterAlpha_.fill(1);
    effectGain_.fill(1);
    smoothing_ = static_cast<float>(1.0 - std::exp(-1.0 / (0.005 * sampleRate_)));
    position_ = toSamples(preserved);
    cursor_ = 0;
    resetCursor();
    playing_ = false;
    master_ = click_ = 0;
    serial_ = 1;
    meters.positionSamples.store(position_);
    meters.positionTicks.store(preserved);
    meters.playing.store(false);
}
bool AudioEngine::publish(const RenderState &state) noexcept
{
    if (state.channelCount > maxChannels || state.eventCount > maxEvents || !validTempo(state.bpm) ||
        state.loopStart < 0 || state.loopEnd <= state.loopStart || state.loopEnd > ppq * 4 * 4096 ||
        state.songEnd <= 0 || state.songEnd > ppq * 4 * 4096 || !std::isfinite(state.masterGain) ||
        state.masterGain < 0 || state.masterGain > 1)
        return false;
    for (const auto &channel : state.channels)
        if (!std::isfinite(channel.gain) || channel.gain < 0 || channel.gain > 1 ||
            !std::isfinite(channel.pan) || channel.pan < -1 || channel.pan > 1 ||
            !std::isfinite(channel.effectGain) || channel.effectGain < 0 || channel.effectGain > 2 ||
            !std::isfinite(channel.filterCutoff) || channel.filterCutoff < 20 ||
            channel.filterCutoff > 20000 || static_cast<int>(channel.instrument) < 0 ||
            static_cast<int>(channel.instrument) > 4)
            return false;
    for (std::size_t i = 0; i < state.eventCount; ++i)
        if (state.events[i].channel >= state.channelCount || state.events[i].pitch > 127 ||
            state.events[i].tick < 0 || state.events[i].tick > ppq * 4 * 4096 ||
            (i > 0 && state.events[i].tick < state.events[i - 1].tick) ||
            !std::isfinite(state.events[i].velocity) || state.events[i].velocity < 0 ||
            state.events[i].velocity > 1)
            return false;
    int index = 0;
    while (retired_.pop(index))
        free_[static_cast<std::size_t>(index)] = true;
    for (std::size_t i = 0; i < free_.size(); ++i)
        if (free_[i])
        {
            states_[i] = state;
            if (!ready_.push(static_cast<int>(i)))
                return false;
            free_[i] = false;
            return true;
        }
    return false;
}
std::int64_t AudioEngine::toSamples(Tick tick) const noexcept
{
    return static_cast<std::int64_t>(std::llround(static_cast<double>(tick) * samplesPerTick_));
}
void AudioEngine::releaseAll() noexcept
{
    for (auto &voice : voices_)
        voice.releasing = true;
}
void AudioEngine::resetCursor() noexcept
{
    cursor_ = 0;
    const auto &state = states_[active_];
    while (cursor_ < state.eventCount && toSamples(state.events[cursor_].tick) < position_)
        ++cursor_;
}
void AudioEngine::noteOn(const ScheduledEvent &event) noexcept
{
    auto voice = std::find_if(voices_.begin(), voices_.end(), [](const Voice &v) { return !v.active; });
    if (voice == voices_.end())
        voice = std::max_element(voices_.begin(), voices_.end(),
                                 [](const Voice &a, const Voice &b) { return a.age < b.age; });
    *voice = {};
    voice->active = true;
    voice->token = event.token;
    voice->channel = event.channel;
    voice->type = states_[active_].channels[event.channel].instrument;
    voice->frequency = 440.0 * std::pow(2.0, (static_cast<double>(event.pitch) - 69.0) / 12.0);
    voice->velocity = event.velocity;
    voice->noise = 0x9e3779b9u ^ serial_++;
}
float AudioEngine::renderVoice(Voice &voice) noexcept
{
    const auto dt = 1.0 / sampleRate_;
    voice.age += dt;
    float signal = 0;
    const auto sine = [](double phase)
    { return static_cast<float>(std::sin(phase * 2.0 * std::numbers::pi)); };
    if (voice.type == Track::Instrument::kick)
    {
        voice.level = static_cast<float>(std::exp(-voice.age * 13.0) * std::min(1.0, voice.age * 1200.0));
        voice.phase += (43.0 + 115.0 * std::exp(-voice.age * 35.0)) * dt;
        signal = sine(voice.phase) * 0.65f;
        if (voice.age > 0.7)
            voice.active = false;
    }
    else if (voice.type == Track::Instrument::snare || voice.type == Track::Instrument::hat)
    {
        voice.noise ^= voice.noise << 13u;
        voice.noise ^= voice.noise >> 17u;
        voice.noise ^= voice.noise << 5u;
        const float noise = static_cast<float>(voice.noise >> 8u) / 8388608.0f - 1.0f;
        const bool hat = voice.type == Track::Instrument::hat;
        voice.level = static_cast<float>(std::exp(-voice.age * (hat ? 65.0 : 22.0)) *
                                         std::min(1.0, voice.age * 3000.0));
        voice.phase += 185.0 * dt;
        signal = hat ? noise * 0.24f : noise * 0.36f + sine(voice.phase) * 0.16f;
        if (voice.age > (hat ? 0.18 : 0.45))
            voice.active = false;
    }
    else
    {
        if (voice.releasing)
            voice.level = std::max(0.0f, voice.level - static_cast<float>(dt / 0.035));
        else if (voice.age < 0.008)
            voice.level = static_cast<float>(voice.age / 0.008);
        else
            voice.level = static_cast<float>(0.65 + 0.35 * std::exp(-(voice.age - 0.008) / 0.12));
        voice.phase += voice.frequency * dt;
        voice.phase -= std::floor(voice.phase);
        signal = voice.type == Track::Instrument::triangle
                     ? static_cast<float>(4.0 * std::abs(voice.phase - 0.5) - 1.0) * 0.22f
                     : sine(voice.phase) * 0.22f;
        if (voice.releasing && voice.level <= 0)
            voice.active = false;
    }
    return signal * voice.level * voice.velocity;
}
void AudioEngine::process(float *const *outputs, int channels, int frames) noexcept
{
    if (!outputs || channels <= 0 || frames <= 0)
        return;
    meters.callbackCount.fetch_add(1, std::memory_order_relaxed);
    for (int channel = 0; channel < channels; ++channel)
        if (outputs[channel])
            std::fill_n(outputs[channel], frames, 0.0f);
    int next = 0;
    while (ready_.pop(next))
    {
        const double tick = samplesPerTick_ > 0 ? static_cast<double>(position_) / samplesPerTick_ : 0;
        const int previous = active_;
        active_ = next;
        const bool tempoChanged = states_[active_].bpm != states_[previous].bpm;
        const bool sequenceChanged = states_[active_].sequenceHash != states_[previous].sequenceHash;
        samplesPerTick_ = sampleRate_ * 60.0 / (states_[active_].bpm * static_cast<double>(ppq));
        if (tempoChanged)
            position_ = static_cast<std::int64_t>(std::llround(tick * samplesPerTick_));
        if (sequenceChanged)
            releaseAll();
        if (tempoChanged || sequenceChanged)
            resetCursor();
        // With three owned slots and a capacity-three queue this cannot overflow.
        const bool retired = retired_.push(previous);
        (void)retired;
    }
    const auto &state = states_[active_];
    const Tick transportEnd = state.loopEnabled ? state.loopEnd : state.songEnd;
    if (const Tick seek = requestedSeek_.exchange(-1, std::memory_order_acq_rel); seek >= 0)
    {
        position_ = toSamples(std::min(seek, transportEnd - 1));
        releaseAll();
        resetCursor();
    }
    const bool requested = requestedPlaying_.load(std::memory_order_acquire);
    if (playing_ && !requested)
    {
        releaseAll();
        position_ = 0;
        cursor_ = 0;
        serial_ = 1;
    }
    if (requested && !playing_ && position_ >= toSamples(transportEnd))
    {
        position_ = 0;
        cursor_ = 0;
        serial_ = 1;
    }
    playing_ = requested;
    const bool solo = std::any_of(state.channels.begin(),
                                  state.channels.begin() + static_cast<std::ptrdiff_t>(state.channelCount),
                                  [](const auto &channel) { return channel.solo; });
    std::array<float, maxChannels> targetL{}, targetR{}, targetAlpha{};
    for (std::size_t c = 0; c < maxChannels; ++c)
    {
        const auto &channel = state.channels[c];
        const float level =
            c >= state.channelCount || channel.mute || (solo && !channel.solo) ? 0.0f : channel.gain;
        targetL[c] = level * std::sqrt((1.0f - channel.pan) * 0.5f);
        targetR[c] = level * std::sqrt((1.0f + channel.pan) * 0.5f);
        targetAlpha[c] = channel.filterEnabled
                             ? static_cast<float>(
                                   1.0 - std::exp(-2.0 * std::numbers::pi *
                                                  std::min<double>(channel.filterCutoff, sampleRate_ * 0.45) /
                                                  sampleRate_))
                             : 1.0f;
    }
    std::array<float, maxChannels> peaks{};
    float peak = 0;
    float leftPeak = 0, rightPeak = 0;
    const auto endSample = toSamples(transportEnd), startSample = toSamples(state.loopStart);
    for (int frame = 0; frame < frames; ++frame)
    {
        if (playing_ && position_ >= endSample)
        {
            if (state.loopEnabled)
            {
                position_ = startSample;
                releaseAll();
                resetCursor();
            }
            else
            {
                playing_ = false;
                requestedPlaying_.store(false, std::memory_order_release);
                releaseAll();
            }
        }
        if (playing_)
        {
            while (cursor_ < state.eventCount && toSamples(state.events[cursor_].tick) <= position_)
            {
                const auto &event = state.events[cursor_++];
                if (event.noteOn)
                    noteOn(event);
                else
                    for (auto &voice : voices_)
                        if (voice.token == event.token)
                            voice.releasing = true;
            }
            if (state.metronome)
            {
                const auto tick = static_cast<Tick>(static_cast<double>(position_) / samplesPerTick_);
                if (position_ == toSamples((tick / ppq + 1) * ppq) ||
                    position_ == toSamples(tick / ppq * ppq))
                    click_ = 0.12f;
            }
        }
        float left = 0, right = 0;
        std::array<float, maxChannels> channelSamples{};
        for (auto &voice : voices_)
            if (voice.active)
            {
                const float sample = renderVoice(voice);
                const auto c = voice.channel;
                channelSamples[c] += sample;
            }
        for (std::size_t c = 0; c < state.channelCount; ++c)
        {
            gainL_[c] += (targetL[c] - gainL_[c]) * smoothing_;
            gainR_[c] += (targetR[c] - gainR_[c]) * smoothing_;
            filterAlpha_[c] += (targetAlpha[c] - filterAlpha_[c]) * smoothing_;
            effectGain_[c] += (state.channels[c].effectGain - effectGain_[c]) * smoothing_;
            filterMemory_[c] += (channelSamples[c] * effectGain_[c] - filterMemory_[c]) * filterAlpha_[c];
            if (std::abs(filterMemory_[c]) < 1.0e-20f)
                filterMemory_[c] = 0;
            left += filterMemory_[c] * gainL_[c];
            right += filterMemory_[c] * gainR_[c];
            peaks[c] = std::max(peaks[c], std::abs(filterMemory_[c]) * std::max(gainL_[c], gainR_[c]));
        }
        clickPhase_ += 1700.0 / sampleRate_;
        clickPhase_ -= std::floor(clickPhase_);
        const float click = click_ * static_cast<float>(std::sin(clickPhase_ * 2.0 * std::numbers::pi));
        click_ *= 0.995f;
        if (click_ < 1.0e-7f)
            click_ = 0;
        master_ += (state.masterGain - master_) * 0.002f;
        left = std::tanh((left + click) * master_);
        right = std::tanh((right + click) * master_);
        peak = std::max({peak, std::abs(left), std::abs(right)});
        leftPeak = std::max(leftPeak, std::abs(left));
        rightPeak = std::max(rightPeak, std::abs(right));
        if (channels == 1)
        {
            if (outputs[0])
                outputs[0][frame] = (left + right) * 0.5f;
        }
        else
        {
            if (outputs[0])
                outputs[0][frame] = left;
            if (outputs[1])
                outputs[1][frame] = right;
        }
        if (playing_)
            ++position_;
    }
    meters.positionSamples.store(position_, std::memory_order_relaxed);
    meters.positionTicks.store(static_cast<Tick>(static_cast<double>(position_) / samplesPerTick_),
                               std::memory_order_relaxed);
    meters.playing.store(playing_, std::memory_order_relaxed);
    meters.masterPeak.store(std::max(peak, meters.masterPeak.load(std::memory_order_relaxed) * 0.86f));
    meters.masterLeft.store(std::max(leftPeak, meters.masterLeft.load(std::memory_order_relaxed) * 0.86f));
    meters.masterRight.store(std::max(rightPeak, meters.masterRight.load(std::memory_order_relaxed) * 0.86f));
    for (std::size_t c = 0; c < maxChannels; ++c)
        meters.channelPeak[c].store(
            std::max(peaks[c], meters.channelPeak[c].load(std::memory_order_relaxed) * 0.86f));
}
} // namespace opendaw
