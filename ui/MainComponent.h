#pragma once
#include "audio/Transport.h"
#include "ui/Theme.h"

namespace opendaw
{
class MainComponent final : public juce::Component
{
  public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress &) override;
    [[nodiscard]] bool runSmokeChecks();

  private:
    void updateTransport();
    StudioLookAndFeel look;
    Transport transport;
    juce::TextButton play{"Play"}, stop{"Stop"};
    juce::Slider tempo;
    juce::ComboBox themes;
    juce::TooltipWindow tooltips{this, 600};
};
} // namespace opendaw
