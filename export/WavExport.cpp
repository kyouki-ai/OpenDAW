#include "export/WavExport.h"
#include "audio/AudioEngine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <memory>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace opendaw
{
namespace
{
void u16(std::ostream &stream, std::uint16_t value)
{
    const char data[]{static_cast<char>(value & 255u), static_cast<char>((value >> 8u) & 255u)};
    stream.write(data, 2);
}
void u32(std::ostream &stream, std::uint32_t value)
{
    const char data[]{static_cast<char>(value & 255u), static_cast<char>((value >> 8u) & 255u),
                      static_cast<char>((value >> 16u) & 255u), static_cast<char>((value >> 24u) & 255u)};
    stream.write(data, 4);
}
struct TempFile
{
    std::filesystem::path path;
    ~TempFile()
    {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};
} // namespace
WavResult exportWav(const Project &source, const std::filesystem::path &destination, int sampleRate, int bits,
                    ExportProgress *progress)
{
    WavResult result;
    result.error = validateProject(source);
    if (!result.error.empty())
        return result;
    if ((sampleRate != 44100 && sampleRate != 48000 && sampleRate != 96000) || (bits != 16 && bits != 24))
    {
        result.error = "Supported export: 44.1/48/96 kHz, PCM 16/24-bit";
        return result;
    }
    auto project = source;
    project.loopStart = 0;
    project.loopEnabled = false;
    project.metronome = false;
    project.loopEnd = ppq * 4;
    for (const auto &track : project.tracks)
        for (const auto &clip : track.clips)
            project.loopEnd = std::max(project.loopEnd, clip.start + clip.length);
    auto state = std::make_unique<RenderState>();
    result.error = compileProject(project, *state);
    if (!result.error.empty())
        return result;
    result.frames = ticksToSamples(project.loopEnd, project.bpm, sampleRate) + sampleRate * 3 / 4;
    if (result.frames > static_cast<std::int64_t>(sampleRate) * 600)
    {
        result.error = "This build supports offline exports up to ten minutes";
        return result;
    }
    if (progress && progress->cancel.load())
    {
        result.error = "Export cancelled";
        return result;
    }
    const auto bytesPerFrame = static_cast<std::uint32_t>(bits / 8 * 2);
    const auto dataSize = static_cast<std::uint32_t>(result.frames * bytesPerFrame);
    TempFile temp{destination};
    temp.path += ".tmp-" + newId().value;
    std::ofstream stream(temp.path, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        result.error = "Cannot create WAV in the selected folder";
        return result;
    }
    stream.write("RIFF", 4);
    u32(stream, dataSize + 36);
    stream.write("WAVEfmt ", 8);
    u32(stream, 16);
    u16(stream, 1);
    u16(stream, 2);
    u32(stream, static_cast<std::uint32_t>(sampleRate));
    u32(stream, static_cast<std::uint32_t>(sampleRate) * bytesPerFrame);
    u16(stream, static_cast<std::uint16_t>(bytesPerFrame));
    u16(stream, static_cast<std::uint16_t>(bits));
    stream.write("data", 4);
    u32(stream, dataSize);
    auto engine = std::make_unique<AudioEngine>();
    engine->prepare(sampleRate);
    if (!engine->publish(*state))
    {
        result.error = "Cannot prepare render state";
        return result;
    }
    engine->play();
    std::array<float, 512> left{}, right{};
    std::array<char, 512 * 6> bytes{};
    float *outputs[]{left.data(), right.data()};
    for (std::int64_t offset = 0; offset < result.frames;)
    {
        if (progress && progress->cancel.load())
        {
            result.error = "Export cancelled";
            return result;
        }
        const auto count = static_cast<int>(std::min<std::int64_t>(512, result.frames - offset));
        engine->process(outputs, 2, count);
        std::size_t cursor = 0;
        const float scale = bits == 24 ? 8388607.0f : 32767.0f;
        for (int frame = 0; frame < count; ++frame)
            for (float value :
                 {left[static_cast<std::size_t>(frame)], right[static_cast<std::size_t>(frame)]})
            {
                if (!std::isfinite(value))
                {
                    result.error = "Render produced a non-finite sample";
                    return result;
                }
                result.peak = std::max(result.peak, std::abs(value));
                const auto sample = static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(std::lround(std::clamp(value, -1.0f, 1.0f) * scale)));
                for (int byte = 0; byte < bits / 8; ++byte)
                    bytes[cursor++] = static_cast<char>((sample >> (byte * 8)) & 255u);
            }
        stream.write(bytes.data(), static_cast<std::streamsize>(cursor));
        if (!stream)
        {
            result.error = "WAV write failed";
            return result;
        }
        offset += count;
        if (progress)
            progress->fraction.store(static_cast<float>(offset) / static_cast<float>(result.frames));
    }
    stream.flush();
    stream.close();
    if (!stream)
    {
        result.error = "WAV flush failed";
        return result;
    }
#if defined(_WIN32)
    if (!MoveFileExW(temp.path.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        result.error =
            "Cannot replace WAV destination (Windows error " + std::to_string(GetLastError()) + ")";
#else
    std::error_code error;
    std::filesystem::rename(temp.path, destination, error);
    if (error)
        result.error = error.message();
#endif
    return result;
}
} // namespace opendaw
