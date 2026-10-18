#pragma once
#include "audio/DeviceHost.h"
#include "ui/Theme.h"

namespace opendaw
{
class AudioSettings final : public juce::Component
{
  public:
    AudioSettings(DeviceHost &host, const ThemeTokens &tokens) : host_(host), tokens_(tokens)
    {
        for (auto *child :
             {static_cast<juce::Component *>(&backend_), static_cast<juce::Component *>(&output_),
              static_cast<juce::Component *>(&rate_), static_cast<juce::Component *>(&buffer_),
              static_cast<juce::Component *>(&apply_), static_cast<juce::Component *>(&status_)})
            addAndMakeVisible(child);
        int id = 1;
        for (auto *type : host_.manager.getAvailableDeviceTypes())
            backend_.addItem(type->getTypeName(), id++);
        backend_.setText(host_.manager.getCurrentAudioDeviceType(), juce::dontSendNotification);
        backend_.onChange = [this]
        {
            host_.manager.setCurrentAudioDeviceType(backend_.getText(), true);
            refresh();
        };
        output_.onChange = [this]
        {
            auto setup = host_.manager.getAudioDeviceSetup();
            setup.outputDeviceName = output_.getText();
            const auto error = host_.manager.setAudioDeviceSetup(setup, true);
            status_.setText(error.isEmpty() ? host_.description() : error, juce::dontSendNotification);
            refreshRates();
        };
        apply_.onClick = [this]
        {
            auto setup = host_.manager.getAudioDeviceSetup();
            setup.sampleRate = rate_.getText().getDoubleValue();
            setup.bufferSize = buffer_.getText().getIntValue();
            const auto error = host_.manager.setAudioDeviceSetup(setup, true);
            status_.setText(error.isEmpty() ? host_.description() : error, juce::dontSendNotification);
            refreshRates();
        };
        refresh();
        status_.setColour(juce::Label::textColourId, tokens_.muted);
        status_.setText(host_.description(), juce::dontSendNotification);
        setSize(520, 350);
    }
    void resized() override
    {
        backend_.setBounds(170, 36, 318, 32);
        output_.setBounds(170, 88, 318, 32);
        rate_.setBounds(170, 140, 148, 32);
        buffer_.setBounds(340, 140, 148, 32);
        apply_.setBounds(340, 206, 148, 36);
        status_.setBounds(24, 266, 472, 64);
    }
    void paint(juce::Graphics &g) override
    {
        g.fillAll(tokens_.background);
        g.setColour(tokens_.text);
        g.setFont(juce::FontOptions(14));
        g.drawText("Audio system", 24, 36, 136, 32, juce::Justification::centredLeft);
        g.drawText("Output device", 24, 88, 136, 32, juce::Justification::centredLeft);
        g.drawText("Rate / buffer", 24, 140, 136, 32, juce::Justification::centredLeft);
        g.setColour(tokens_.muted);
        g.setFont(juce::FontOptions(12));
        g.drawText("Shared output. Audio input and recording are disabled.", 24, 245, 472, 24,
                   juce::Justification::centredLeft);
    }

  private:
    void refresh()
    {
        output_.clear(juce::dontSendNotification);
        if (auto *type = host_.manager.getCurrentDeviceTypeObject())
        {
            type->scanForDevices();
            output_.addItemList(type->getDeviceNames(false), 1);
        }
        output_.setText(host_.manager.getAudioDeviceSetup().outputDeviceName, juce::dontSendNotification);
        refreshRates();
    }
    void refreshRates()
    {
        rate_.clear(juce::dontSendNotification);
        buffer_.clear(juce::dontSendNotification);
        if (auto *device = host_.manager.getCurrentAudioDevice())
        {
            int id = 1;
            for (const auto rate : device->getAvailableSampleRates())
                rate_.addItem(juce::String(rate, 0), id++);
            id = 1;
            for (const auto size : device->getAvailableBufferSizes())
                buffer_.addItem(juce::String(size), id++);
            rate_.setText(juce::String(device->getCurrentSampleRate(), 0), juce::dontSendNotification);
            buffer_.setText(juce::String(device->getCurrentBufferSizeSamples()), juce::dontSendNotification);
        }
    }
    DeviceHost &host_;
    const ThemeTokens &tokens_;
    juce::ComboBox backend_, output_, rate_, buffer_;
    juce::TextButton apply_{"Apply"};
    juce::Label status_;
};
} // namespace opendaw
