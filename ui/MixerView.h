#pragma once
#include "audio/AudioEngine.h"
#include "ui/Theme.h"
#include <functional>

namespace opendaw
{
class MixerView final : public juce::Component
{
  public:
    MixerView(Project &, AudioEngine &, const ThemeTokens &);
    ~MixerView() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    void refresh();
    bool exerciseEditing();
    std::function<void()> beforeEdit, onEdit;

  private:
    class Strip;
    Project &project_;
    AudioEngine &engine_;
    const ThemeTokens &theme_;
    juce::Viewport viewport_;
    juce::Component content_;
    std::vector<std::unique_ptr<Strip>> strips_;
};
} // namespace opendaw
