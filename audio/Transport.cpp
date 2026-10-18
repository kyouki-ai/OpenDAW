#include "audio/Transport.h"
namespace opendaw
{
bool Transport::setTempo(double bpm) noexcept
{
    if (!validTempo(bpm))
        return false;
    bpm_ = bpm;
    return true;
}
void Transport::play() noexcept
{
    playing_ = true;
}
void Transport::stop() noexcept
{
    playing_ = false;
    position_ = 0;
}
bool Transport::isPlaying() const noexcept
{
    return playing_;
}
double Transport::tempo() const noexcept
{
    return bpm_;
}
Tick Transport::position() const noexcept
{
    return position_;
}
} // namespace opendaw
