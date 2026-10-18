#include "audio/AudioEngine.h"
#include "export/WavExport.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>
#include <thread>
#include <vector>
#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

namespace
{
thread_local bool auditAllocation = false;
std::size_t allocations = 0;
int failures = 0;
void check(bool condition, const char *name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    if (!condition)
        ++failures;
}
std::vector<float> render(const opendaw::RenderState &state, int blockSize, int frames)
{
    auto engine = std::make_unique<opendaw::AudioEngine>();
    engine->prepare(48000);
    check(engine->publish(state), "publish validated render state");
    engine->play();
    std::vector<float> data(static_cast<std::size_t>(frames) * 2);
    for (int offset = 0; offset < frames; offset += blockSize)
    {
        float *outputs[]{data.data() + offset, data.data() + frames + offset};
        auditAllocation = true;
        engine->process(outputs, 2, std::min(blockSize, frames - offset));
        auditAllocation = false;
    }
    return data;
}
} // namespace
void *operator new(std::size_t size)
{
    if (auditAllocation)
        ++allocations;
    if (auto *memory = std::malloc(size == 0 ? 1 : size))
        return memory;
    throw std::bad_alloc();
}
void operator delete(void *memory) noexcept
{
    std::free(memory);
}
void operator delete(void *memory, std::size_t) noexcept
{
    std::free(memory);
}
void *operator new[](std::size_t size)
{
    return ::operator new(size);
}
void operator delete[](void *memory) noexcept
{
    ::operator delete(memory);
}
void operator delete[](void *memory, std::size_t) noexcept
{
    ::operator delete(memory);
}

int runTests()
{
    using namespace opendaw;
    auto project = makeDemoProject();
    check(validateProject(project).empty(), "demo model is valid");
    check(validId(project.id) && newId() != project.id, "generated UUID identity");
    auto invalid = project;
    invalid.patterns[0].notes[1].id = invalid.patterns[0].notes[0].id;
    check(!validateProject(invalid).empty(), "duplicate note ID rejected");
    invalid = project;
    invalid.tracks[0].clips[0].patternId = newId();
    check(!validateProject(invalid).empty(), "missing pattern reference rejected");
    invalid = project;
    invalid.tracks[0].gain = NAN;
    check(!validateProject(invalid).empty(), "NaN channel gain rejected");
    auto state = std::make_unique<RenderState>();
    check(compileProject(project, *state).empty() && state->eventCount > 0, "compile arrangement events");
    bool sorted = true;
    for (std::size_t i = 1; i < state->eventCount; ++i)
        sorted = sorted && state->events[i - 1].tick <= state->events[i].tick;
    check(sorted, "events ordered by absolute musical time");
    auto engine = std::make_unique<AudioEngine>();
    std::array<float, 512> left{}, right{}, extra{};
    float *outputs[]{left.data(), right.data(), extra.data()};
    std::fill(left.begin(), left.end(), 1.0f);
    engine->process(outputs, 3, 512);
    check(std::all_of(left.begin(), left.end(), [](float v) { return v == 0; }),
          "stopped engine writes silence");
    check(engine->publish(*state) && engine->publish(*state) && !engine->publish(*state),
          "bounded snapshot ownership");
    engine->process(outputs, 3, 512);
    check(engine->publish(*state), "retired snapshots returned to UI");
    engine->play();
    engine->process(outputs, 3, 512);
    check(engine->meters.positionSamples.load() == 512 && engine->meters.playing.load(),
          "sample-driven transport");
    check(std::all_of(extra.begin(), extra.end(), [](float v) { return v == 0; }),
          "unused output channel cleared");
    engine->seek(ppq);
    engine->process(outputs, 2, 1);
    check(std::abs(engine->meters.positionSamples.load() - ticksToSamples(ppq, project.bpm, 48000)) <= 1,
          "seek uses musical ticks");
    engine->stop();
    for (int i = 0; i < 100; ++i)
        engine->process(outputs, 2, 512);
    check(!engine->meters.playing.load() && engine->meters.positionSamples.load() == 0, "stop resets clock");
    check(std::all_of(left.begin(), left.end(), [](float v) { return std::abs(v) < 1.0e-7f; }),
          "stop drains instrument tails");
    engine->seek(ppq);
    engine->process(outputs, 2, 1);
    engine->stop();
    engine->process(outputs, 2, 1);
    check(engine->meters.positionSamples.load() == 0, "stop returns to start after seeking while stopped");
    auto a = render(*state, 64, 96000);
    auto b = render(*state, 257, 96000);
    check(a == b, "sample-exact render independent of callback block size");
    float peak = 0;
    bool finite = true;
    for (float sample : a)
    {
        finite = finite && std::isfinite(sample);
        peak = std::max(peak, std::abs(sample));
    }
    check(finite && peak > 0.01f && peak < 0.98f, "demo produces finite non-clipping PCM");
    for (auto &channel : state->channels)
        channel.mute = true;
    const auto silence = render(*state, 128, 2048);
    check(std::all_of(silence.begin(), silence.end(), [](float v) { return v == 0; }), "mute all is silent");
    for (auto &channel : state->channels)
    {
        channel.mute = false;
        channel.pan = 1;
    }
    const auto panned = render(*state, 128, 4096);
    check(std::all_of(panned.begin(), panned.begin() + 4096, [](float v) { return v == 0; }),
          "hard right pan silences left");
    check(allocations == 0, "zero operator-new calls inside process");
    engine = std::make_unique<AudioEngine>();
    check(compileProject(project, *state).empty() && engine->publish(*state), "loop state ready");
    engine->play();
    const auto end = ticksToSamples(project.loopEnd, project.bpm, 48000);
    for (std::int64_t done = 0; done < end + 128;)
    {
        const int count = static_cast<int>(std::min<std::int64_t>(512, end + 128 - done));
        engine->process(outputs, 2, count);
        done += count;
    }
    check(engine->meters.positionSamples.load() == 128, "exact loop boundary wraps without lost samples");
    state->channels[0].pan = NAN;
    check(!engine->publish(*state), "unsafe render snapshot rejected before callback");
    check(compileProject(project, *state).empty(), "restore valid state");
    auto reference = std::make_unique<AudioEngine>();
    engine = std::make_unique<AudioEngine>();
    check(reference->publish(*state) && engine->publish(*state), "continuous playback states prepared");
    reference->play();
    engine->play();
    std::array<float, 512> referenceLeft{}, referenceRight{};
    float *referenceOutputs[]{referenceLeft.data(), referenceRight.data()};
    reference->process(referenceOutputs, 2, 512);
    engine->process(outputs, 2, 512);
    check(engine->publish(*state), "publish unchanged sequence while playing");
    reference->process(referenceOutputs, 2, 512);
    engine->process(outputs, 2, 512);
    check(referenceLeft == left && referenceRight == right,
          "unchanged snapshot preserves active voices and scheduler position");
    const auto beforeTempo = engine->meters.positionSamples.load();
    state->bpm *= 2;
    check(engine->publish(*state), "publish tempo change while playing");
    engine->process(outputs, 2, 1);
    check(engine->meters.positionSamples.load() == beforeTempo / 2 + 1,
          "tempo change preserves fractional musical position");
    auto filteredProject = makeDemoProject();
    filteredProject.tracks.resize(1);
    filteredProject.patterns.resize(1);
    filteredProject.tracks[0].instrument = Track::Instrument::sine;
    filteredProject.patterns[0].notes = {{newId(), 0, ppq * 4, 96, 1}};
    check(compileProject(filteredProject, *state).empty(), "filter reference compiled");
    const auto dry = render(*state, 128, 16384);
    filteredProject.tracks[0].filterEnabled = true;
    filteredProject.tracks[0].filterCutoff = 250;
    check(compileProject(filteredProject, *state).empty(), "low-pass state compiled");
    const auto wet = render(*state, 257, 16384);
    double dryEnergy = 0, wetEnergy = 0;
    for (int i = 4096; i < 16384; ++i)
    {
        dryEnergy += dry[static_cast<std::size_t>(i)] * dry[static_cast<std::size_t>(i)];
        wetEnergy += wet[static_cast<std::size_t>(i)] * wet[static_cast<std::size_t>(i)];
    }
    check(dryEnergy > 0 && wetEnergy < dryEnergy * 0.03, "low-pass attenuates high notes after settling");
    check(render(*state, 64, 4096) == render(*state, 193, 4096),
          "effect chain remains independent of callback size");
    state->channels[0].filterCutoff = NAN;
    check(!engine->publish(*state), "non-finite filter rejected before callback");
    check(allocations == 0, "filter and mixer allocate no memory inside process");
    SpscQueue<std::uint32_t, 8> queue;
    constexpr std::uint32_t transfers = 10000;
    std::jthread producer(
        [&queue]
        {
            for (std::uint32_t i = 0; i < transfers; ++i)
                while (!queue.push(i))
                    std::this_thread::yield();
        });
    bool ordered = true;
    for (std::uint32_t expected = 0; expected < transfers; ++expected)
    {
        std::uint32_t value = 0;
        while (!queue.pop(value))
            std::this_thread::yield();
        ordered = ordered && value == expected;
    }
    producer.join();
    check(ordered, "bounded SPSC preserves order across concurrent producer and consumer");
    check(compileProject(project, *state).empty(), "prepare non-loop transport");
    state->loopEnabled = false;
    state->loopEnd = ppq;
    state->songEnd = ppq;
    engine = std::make_unique<AudioEngine>();
    check(engine->publish(*state), "publish non-loop transport");
    engine->play();
    for (int i = 0; i < 60; ++i)
        engine->process(outputs, 2, 512);
    check(!engine->meters.playing.load(), "non-loop playback stops at region end");
    engine->play();
    engine->process(outputs, 2, 1);
    check(engine->meters.positionSamples.load() == 1 && engine->meters.playing.load(),
          "play restarts after reaching non-loop end");
    auto shortLoop = project;
    shortLoop.loopEnd = ppq;
    shortLoop.loopEnabled = false;
    check(compileProject(shortLoop, *state).empty() && state->songEnd == project.loopEnd &&
              state->events[state->eventCount - 1].tick > shortLoop.loopEnd,
          "disabling a short loop restores the entire arrangement schedule");
    const auto wav = std::filesystem::current_path() / ("opendaw-test-" + newId().value + ".wav");
    const auto exported = exportWav(project, wav, 48000, 24);
    if (!exported.error.empty())
        std::cerr << "Export error: " << exported.error << '\n';
    check(exported.error.empty() && exported.peak > 0.01f && exported.peak < 0.98f,
          "24-bit offline export produces non-clipping signal");
    if (!exported.error.empty())
        return 1;
    std::ifstream input(wav, std::ios::binary);
    std::array<unsigned char, 44> header{};
    input.read(reinterpret_cast<char *>(header.data()), 44);
    check(header[0] == 'R' && header[8] == 'W' && header[22] == 2 && header[34] == 24,
          "WAV RIFF format, stereo and bit depth verified");
    check(compileProject(project, *state).empty(), "offline reference prepared");
    const auto referencePcm = render(*state, 193, 4096);
    bool equivalent = true;
    for (int frame = 0; frame < 4096; ++frame)
        for (int channel = 0; channel < 2; ++channel)
        {
            unsigned char sample[3]{};
            input.read(reinterpret_cast<char *>(sample), 3);
            std::int32_t pcm = sample[0] | (static_cast<std::int32_t>(sample[1]) << 8) |
                               (static_cast<std::int32_t>(sample[2]) << 16);
            if (pcm & 0x800000)
                pcm -= 0x1000000;
            const auto expected = static_cast<std::int32_t>(
                std::lround(referencePcm[static_cast<std::size_t>(channel * 4096 + frame)] * 8388607.0f));
            equivalent = equivalent && input.good() && pcm == expected;
        }
    check(equivalent, "exported 24-bit PCM matches live engine sample for sample");
    input.close();
    check(std::filesystem::file_size(wav) == static_cast<std::uintmax_t>(44 + exported.frames * 6),
          "WAV payload size matches rendered frame count");
    ExportProgress cancelled;
    cancelled.cancel.store(true);
    const auto cancelledResult = exportWav(project, wav, 48000, 24, &cancelled);
    check(!cancelledResult.error.empty() &&
              std::filesystem::file_size(wav) == static_cast<std::uintmax_t>(44 + exported.frames * 6),
          "cancelled export preserves existing destination");
    const auto sixteen = exportWav(project, wav, 44100, 16);
    check(sixteen.error.empty() &&
              std::filesystem::file_size(wav) == static_cast<std::uintmax_t>(44 + sixteen.frames * 4),
          "16-bit 44.1-kHz export replaces file atomically");
    check(!exportWav(project, wav.parent_path() / "missing-folder" / "test.wav").error.empty(),
          "unwritable export returns an error without terminating");
    std::error_code cleanupError;
    std::filesystem::remove(wav, cleanupError);
    return failures == 0 ? 0 : 1;
}

int main()
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
#if defined(_MSC_VER)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _set_error_mode(_OUT_TO_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    try
    {
        return runTests();
    }
    catch (const std::exception &error)
    {
        std::cerr << "FAIL unhandled test exception: " << error.what() << '\n';
        return 1;
    }
}
