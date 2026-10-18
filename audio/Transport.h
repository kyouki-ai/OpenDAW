#pragma once
#include "core/Project.h"

namespace opendaw
{
// Message-thread preview transport. No device or real-time clock in slice 01.
class Transport
{
  public:
    bool setTempo(double bpm) noexcept;
    void play() noexcept;
    void stop() noexcept;
    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] double tempo() const noexcept;
    [[nodiscard]] Tick position() const noexcept;

  private:
    double bpm_ = 124.0;
    bool playing_ = false;
    Tick position_ = 0;
};
} // namespace opendaw
