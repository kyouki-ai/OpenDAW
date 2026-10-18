#pragma once
#include "core/Project.h"
#include <atomic>
#include <filesystem>

namespace opendaw
{
struct ExportProgress
{
    std::atomic<bool> cancel{false};
    std::atomic<float> fraction{0};
};
struct WavResult
{
    std::string error;
    std::int64_t frames = 0;
    float peak = 0;
};
// Offline only. Uses the same scheduler/synth as real-time playback.
[[nodiscard]] WavResult exportWav(const Project &, const std::filesystem::path &, int sampleRate = 48000,
                                  int bits = 24, ExportProgress * = nullptr);
} // namespace opendaw
