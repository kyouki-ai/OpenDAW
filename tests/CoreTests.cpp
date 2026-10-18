#include "audio/Transport.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
int failures = 0;
void check(bool condition, const char *name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    if (!condition)
        ++failures;
}
template <typename Exception, typename Fn> void throws(Fn fn, const char *name)
{
    try
    {
        fn();
        check(false, name);
    }
    catch (const Exception &)
    {
        check(true, name);
    }
    catch (...)
    {
        check(false, name);
    }
}
} // namespace
int main()
{
    using namespace opendaw;
    check(ticksToSamples(ppq, 120, 48000) == 24000, "quarter note at 120 BPM / 48 kHz");
    check(ticksToSamples(ppq * 4, 120, 44100) == 88200, "bar at 44.1 kHz");
    check(ticksToSamples(0, 124, 48000) == 0, "origin");
    check(ticksToSamples(1, 120, 48000) == 25, "single tick");
    check(ticksToSamples(1, 124, 44100) == 22, "fractional sample rounding");
    throws<std::invalid_argument>([] { (void)ticksToSamples(-1, 120, 48000); }, "negative ticks");
    throws<std::invalid_argument>([] { (void)ticksToSamples(1, 0, 48000); }, "zero tempo");
    throws<std::invalid_argument>([] { (void)ticksToSamples(1, 120, NAN); }, "NaN sample rate");
    throws<std::overflow_error>([] { (void)ticksToSamples(INT64_MAX, 20, 192000); }, "sample overflow");
    Transport transport;
    check(!transport.isPlaying() && transport.position() == 0, "transport initial state");
    check(transport.setTempo(20) && transport.setTempo(300), "tempo inclusive boundaries");
    check(!transport.setTempo(NAN) && !transport.setTempo(301) && transport.tempo() == 300,
          "invalid tempo preserves previous value");
    transport.play();
    transport.play();
    check(transport.isPlaying(), "play is idempotent");
    transport.stop();
    check(!transport.isPlaying() && transport.position() == 0, "stop resets transport");
    const EntityId id{"9646eac2-42a2-4a63-8f59-e071284197a8"};
    auto project = Project{id};
    const auto copied = project;
    check(copied.id == id && copied.schemaVersion == 1, "project copy preserves identity");
    return failures == 0 ? 0 : 1;
}
