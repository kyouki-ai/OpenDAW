#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace opendaw
{
struct ThemeTokens
{
    juce::Colour background, surface, raised, border, text, muted, accent, secondary;
    static ThemeTokens graphite();
    static ThemeTokens ultraviolet();
    static ThemeTokens ember();
};
class StudioLookAndFeel final : public juce::LookAndFeel_V4
{
  public:
    StudioLookAndFeel();
    void setTheme(ThemeTokens next);
    ThemeTokens tokens;
    void drawButtonBackground(juce::Graphics &, juce::Button &, const juce::Colour &, bool, bool) override;
    void drawButtonText(juce::Graphics &, juce::TextButton &, bool, bool) override;
};
} // namespace opendaw
