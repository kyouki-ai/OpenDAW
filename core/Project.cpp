#include "core/Project.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace opendaw
{
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
