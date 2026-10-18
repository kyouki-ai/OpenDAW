#include "audio/DeviceHost.h"
#include <chrono>

namespace opendaw
{
juce::String DeviceHost::start(const juce::XmlElement *savedState)
{
    const auto error = manager.initialise(0, 2, savedState, true);
    if (error.isNotEmpty())
        return error;
    manager.addAudioCallback(this);
    registered_ = true;
    return {};
}
void DeviceHost::shutdown()
{
    if (registered_)
        manager.removeAudioCallback(this);
    registered_ = false;
    manager.closeAudioDevice();
}
juce::String DeviceHost::description() const
{
    if (auto *device = manager.getCurrentAudioDevice())
        return device->getName() + "  /  " + juce::String(device->getCurrentSampleRate() / 1000.0, 1) +
               " kHz  /  " + juce::String(device->getCurrentBufferSizeSamples()) + " samples";
    return "No audio output - open Audio settings";
}
void DeviceHost::audioDeviceAboutToStart(juce::AudioIODevice *device)
{
    rate_ = device->getCurrentSampleRate();
    failed_.store(false);
    engine_.prepare(rate_);
}
void DeviceHost::audioDeviceStopped()
{
    engine_.meters.playing.store(false);
}
void DeviceHost::audioDeviceIOCallbackWithContext(const float *const *, int, float *const *outputs,
                                                  int channels, int frames,
                                                  const juce::AudioIODeviceCallbackContext &)
{
    const auto start = std::chrono::steady_clock::now();
    engine_.process(outputs, channels, frames);
    const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const auto load = frames > 0 ? static_cast<float>(seconds * rate_ / frames) : 0.0f;
    engine_.meters.cpuLoad.store(load, std::memory_order_relaxed);
    if (load > 1.0f)
        engine_.meters.deadlineOverruns.fetch_add(1, std::memory_order_relaxed);
}
} // namespace opendaw
