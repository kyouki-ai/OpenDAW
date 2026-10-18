#include "ui/Theme.h"
namespace opendaw
{
ThemeTokens ThemeTokens::graphite()
{
    return {juce::Colour(0xff101419), juce::Colour(0xff181e25), juce::Colour(0xff222b34),
            juce::Colour(0xff303c48), juce::Colour(0xffe8edf2), juce::Colour(0xffa5b1bf),
            juce::Colour(0xff6fe3c0), juce::Colour(0xffbba3ff)};
}
ThemeTokens ThemeTokens::ultraviolet()
{
    return {juce::Colour(0xff11101b), juce::Colour(0xff1b182b), juce::Colour(0xff29243d),
            juce::Colour(0xff3a3352), juce::Colour(0xfff2efff), juce::Colour(0xffb6accd),
            juce::Colour(0xffbda2ff), juce::Colour(0xff7edce8)};
}
ThemeTokens ThemeTokens::ember()
{
    return {juce::Colour(0xff181412), juce::Colour(0xff231e1a), juce::Colour(0xff302923),
            juce::Colour(0xff493c31), juce::Colour(0xfff4eee7), juce::Colour(0xffbfb0a2),
            juce::Colour(0xffffbd7a), juce::Colour(0xff94c9b7)};
}
StudioLookAndFeel::StudioLookAndFeel()
{
    setTheme(ThemeTokens::ember());
}
void StudioLookAndFeel::setTheme(ThemeTokens next)
{
    tokens = next;
    setColour(juce::TextButton::textColourOffId, tokens.text);
    setColour(juce::TextButton::textColourOnId, tokens.background);
    setColour(juce::ComboBox::backgroundColourId, tokens.raised);
    setColour(juce::ComboBox::textColourId, tokens.text);
    setColour(juce::ComboBox::outlineColourId, tokens.border);
    setColour(juce::PopupMenu::backgroundColourId, tokens.surface);
    setColour(juce::PopupMenu::textColourId, tokens.text);
    setColour(juce::Slider::textBoxTextColourId, tokens.text);
    setColour(juce::Slider::textBoxBackgroundColourId, tokens.raised);
    setColour(juce::Slider::textBoxOutlineColourId, tokens.border);
}
void StudioLookAndFeel::drawButtonBackground(juce::Graphics &g, juce::Button &b, const juce::Colour &,
                                             bool hover, bool down)
{
    const auto bounds = b.getLocalBounds().toFloat().reduced(1.0f);
    const bool isPlay = b.getComponentID() == "transport-play";
    const float radius = isPlay ? bounds.getHeight() * 0.5f : 7.0f;
    auto colour = isPlay || b.getToggleState() ? tokens.accent : tokens.raised;
    if (down)
        colour = colour.darker(0.15f);
    else if (hover)
        colour = colour.brighter(0.12f);
    g.setColour(colour.withMultipliedAlpha(b.isEnabled() ? 1.0f : 0.4f));
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(b.hasKeyboardFocus(true) ? tokens.accent : tokens.border);
    g.drawRoundedRectangle(bounds, radius, b.hasKeyboardFocus(true) ? 2.0f : 1.0f);
}
void StudioLookAndFeel::drawButtonText(juce::Graphics &g, juce::TextButton &b, bool hover, bool down)
{
    const auto centre = b.getLocalBounds().toFloat().getCentre();
    if (b.getComponentID() == "transport-play")
    {
        g.setColour(tokens.background);
        juce::Path triangle;
        triangle.addTriangle(centre.x - 4.0f, centre.y - 6.0f, centre.x - 4.0f, centre.y + 6.0f,
                             centre.x + 6.0f, centre.y);
        g.fillPath(triangle);
    }
    else if (b.getComponentID() == "transport-stop")
    {
        g.setColour(tokens.text);
        g.fillRoundedRectangle(centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f, 1.5f);
    }
    else
        juce::LookAndFeel_V4::drawButtonText(g, b, hover, down);
}
} // namespace opendaw
