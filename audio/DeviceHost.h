#pragma once
#include "audio/AudioEngine.h"
#include <juce_audio_devices/juce_audio_devices.h>

namespace opendaw
{
class DeviceHost final : private juce::AudioIODeviceCallback
{
  public:
    explicit DeviceHost(AudioEngine &engine) : engine_(engine) {}
    ~DeviceHost() override
    {
        shutdown();
    }
    juce::String start(const juce::XmlElement *savedState = nullptr);
    void shutdown();
    [[nodiscard]] juce::String description() const;
    [[nodiscard]] bool available() const
    {
        return manager.getCurrentAudioDevice() != nullptr && !failed_.load();
    }
    juce::AudioDeviceManager manager;

  private:
    void audioDeviceAboutToStart(juce::AudioIODevice *) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String &) override
    {
        failed_.store(true);
    }
    void audioDeviceIOCallbackWithContext(const float *const *, int, float *const *, int, int,
                                          const juce::AudioIODeviceCallbackContext &) override;
    AudioEngine &engine_;
    bool registered_ = false;
    std::atomic<bool> failed_{false};
    double rate_ = 48000;
};
} // namespace opendaw
