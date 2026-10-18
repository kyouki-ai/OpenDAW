#include "ui/MainComponent.h"

namespace opendaw
{
namespace
{
void text(juce::Graphics &g, const juce::String &value, juce::Rectangle<int> area, float size,
          juce::Colour colour, bool bold = false,
          juce::Justification align = juce::Justification::centredLeft)
{
    g.setColour(colour);
    g.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
    g.drawText(value, area, align);
}
} // namespace
MainComponent::MainComponent()
{
    setLookAndFeel(&look);
    setWantsKeyboardFocus(true);
    for (auto *child : {static_cast<juce::Component *>(&play), static_cast<juce::Component *>(&stop),
                        static_cast<juce::Component *>(&tempo), static_cast<juce::Component *>(&themes)})
        addAndMakeVisible(child);
    play.setTooltip("Preview transport state / Space. Audio is not connected yet.");
    play.setComponentID("transport-play");
    play.setTitle("Play preview transport");
    stop.setTooltip("Stop and reset / Escape");
    stop.setComponentID("transport-stop");
    stop.setTitle("Stop preview transport");
    play.onClick = [this]
    {
        transport.play();
        updateTransport();
    };
    stop.onClick = [this]
    {
        transport.stop();
        updateTransport();
    };
    tempo.setSliderStyle(juce::Slider::IncDecButtons);
    tempo.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 65, 32);
    tempo.setRange(20, 300, 1);
    tempo.setValue(transport.tempo(), juce::dontSendNotification);
    tempo.setTitle("Tempo in beats per minute");
    tempo.setTooltip("Project tempo: 20 to 300 BPM");
    tempo.onValueChange = [this]
    {
        (void)transport.setTempo(tempo.getValue());
        repaint();
    };
    themes.addItem("Graphite / Mint", 1);
    themes.addItem("Ultraviolet / Ice", 2);
    themes.addItem("Ember / Sand", 3);
    themes.setSelectedId(3, juce::dontSendNotification);
    themes.setTitle("Visual direction");
    themes.onChange = [this]
    {
        look.setTheme(themes.getSelectedId() == 2   ? ThemeTokens::ultraviolet()
                      : themes.getSelectedId() == 3 ? ThemeTokens::ember()
                                                    : ThemeTokens::graphite());
        sendLookAndFeelChange();
        repaint();
    };
    setSize(1280, 800);
}
MainComponent::~MainComponent()
{
    setLookAndFeel(nullptr);
}
void MainComponent::updateTransport()
{
    play.setToggleState(transport.isPlaying(), juce::dontSendNotification);
    repaint();
}
void MainComponent::paint(juce::Graphics &g)
{
    const auto &t = look.tokens;
    const int w = getWidth(), h = getHeight();
    g.fillAll(t.background);
    g.setGradientFill(
        juce::ColourGradient(t.surface.brighter(0.025f), 0.0f, 0.0f, t.background, 0.0f, 88.0f, false));
    g.fillRect(0, 0, w, 88);
    g.setColour(t.border);
    g.drawHorizontalLine(87, 0.0f, static_cast<float>(w));
    g.setColour(t.accent);
    juce::Path wave;
    wave.startNewSubPath(25.0f, 43.0f);
    wave.cubicTo(31.0f, 43.0f, 29.0f, 31.0f, 34.0f, 31.0f);
    wave.cubicTo(39.0f, 31.0f, 37.0f, 55.0f, 43.0f, 55.0f);
    wave.cubicTo(49.0f, 55.0f, 47.0f, 22.0f, 53.0f, 22.0f);
    wave.cubicTo(59.0f, 22.0f, 57.0f, 55.0f, 63.0f, 55.0f);
    wave.cubicTo(69.0f, 55.0f, 67.0f, 34.0f, 72.0f, 34.0f);
    wave.cubicTo(76.0f, 34.0f, 75.0f, 43.0f, 79.0f, 43.0f);
    g.strokePath(wave,
                 juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    text(g, "OpenDAW", {92, 24, 118, 28}, 22, t.text);
    text(g, "STUDIO PREVIEW", {93, 52, 130, 16}, 9, t.muted);
    const int transportX = w >= 1100 ? 460 : 292;
    text(g, "BPM", {transportX + 130, 12, 100, 18}, 10, t.muted);
    text(g, "001 : 01 : 000", {transportX + 270, 27, 155, 30}, 20, t.text);
    text(g, "BAR      BEAT      TICK", {transportX + 270, 56, 155, 16}, 9, t.muted);
    if (w >= 1100)
    {
        g.setColour(t.border);
        g.drawVerticalLine(226, 22.0f, 62.0f);
        text(g, "Untitled session", {250, 30, 190, 26}, 15, t.text);
        text(g, "4 / 4", {transportX + 450, 32, 60, 20}, 13, t.muted);
    }

    const int side = w >= 1100 ? 220 : 180;
    g.setColour(t.surface);
    g.fillRoundedRectangle(16.0f, 104.0f, static_cast<float>(side), static_cast<float>(h - 152), 10.0f);
    g.setColour(t.border.withAlpha(0.5f));
    g.drawRoundedRectangle(16.5f, 104.5f, static_cast<float>(side - 1), static_cast<float>(h - 153), 10.0f,
                           1.0f);
    text(g, "SESSION", {32, 122, 150, 22}, 11, t.muted, true);
    text(g, "Untitled session", {32, 158, side - 24, 26}, 17, t.text, true);
    text(g, "Local workspace", {32, 188, side - 24, 20}, 12, t.muted);
    g.setColour(t.border);
    g.drawHorizontalLine(228, 32.0f, static_cast<float>(side));
    text(g, "YOUR LIBRARY", {32, 250, 150, 22}, 11, t.muted, true);
    text(g, "No instruments yet", {32, 290, side - 24, 22}, 13, t.muted);
    g.setColour(t.secondary.withAlpha(0.12f));
    g.fillRoundedRectangle(32.0f, 335.0f, static_cast<float>(side - 32), 44.0f, 6.0f);
    g.setColour(t.secondary);
    g.fillEllipse(44.0f, 354.0f, 6.0f, 6.0f);
    text(g, "Local project", {60, 344, side - 60, 24}, 12, t.secondary);
    text(g, "FOUNDATION  /  01", {32, h - 105, side - 24, 22}, 10, t.accent);
    text(g, "Windows desktop preview", {32, h - 80, side - 24, 20}, 11, t.muted);

    const int x = side + 32, right = w - 16, top = 162, bottom = h - 65;
    text(g, "Arrangement", {x + 8, 111, 220, 28}, 20, t.text, true);
    text(g, "SESSION 01   /   EMPTY PROJECT", {x + 225, 116, 300, 20}, 10, t.muted);
    g.setColour(t.surface);
    g.fillRoundedRectangle(static_cast<float>(x), static_cast<float>(top), static_cast<float>(right - x),
                           static_cast<float>(bottom - top), 10.0f);
    g.setColour(t.border.withAlpha(0.7f));
    g.drawRoundedRectangle(static_cast<float>(x) + 0.5f, static_cast<float>(top) + 0.5f,
                           static_cast<float>(right - x) - 1.0f, static_cast<float>(bottom - top) - 1.0f,
                           10.0f, 1.0f);
    const int gridStart = x + 55;
    for (int bar = 0; bar < 9; ++bar)
    {
        const int bx = gridStart + bar * (right - gridStart) / 8;
        g.setColour(t.border.withAlpha(0.5f));
        g.drawVerticalLine(bx, static_cast<float>(top + 34), static_cast<float>(bottom - 12));
        if (bar < 8)
        {
            const int beatWidth = (right - gridStart) / 32;
            for (int beat = 1; beat < 4; ++beat)
            {
                g.setColour(t.border.withAlpha(0.16f));
                g.drawVerticalLine(bx + beatWidth * beat, static_cast<float>(top + 34),
                                   static_cast<float>(bottom - 12));
            }
        }
        if (bar < 8)
            text(g, juce::String(bar + 1).paddedLeft('0', 2), {bx + 8, top + 7, 40, 20}, 10, t.muted);
    }
    for (int row = top + 34; row < bottom - 12; row += 64)
    {
        g.setColour(t.border.withAlpha(0.35f));
        g.drawHorizontalLine(row, static_cast<float>(x + 12), static_cast<float>(right - 12));
    }
    const int cx = (x + right) / 2, cy = (top + bottom) / 2;
    g.setColour(t.accent.withAlpha(0.65f));
    g.drawVerticalLine(gridStart, static_cast<float>(top + 31), static_cast<float>(bottom - 12));
    juce::Path marker;
    marker.addTriangle(static_cast<float>(gridStart - 5), static_cast<float>(top + 27),
                       static_cast<float>(gridStart + 5), static_cast<float>(top + 27),
                       static_cast<float>(gridStart), static_cast<float>(top + 35));
    g.fillPath(marker);
    g.setColour(t.background.withAlpha(0.96f));
    g.fillRoundedRectangle(static_cast<float>(cx - 225), static_cast<float>(cy - 100), 450.0f, 200.0f, 16.0f);
    g.setColour(t.accent);
    g.drawEllipse(static_cast<float>(cx - 17), static_cast<float>(cy - 72), 34.0f, 34.0f, 1.5f);
    g.drawLine(static_cast<float>(cx - 6), static_cast<float>(cy - 55), static_cast<float>(cx + 6),
               static_cast<float>(cy - 55), 1.5f);
    g.drawLine(static_cast<float>(cx), static_cast<float>(cy - 61), static_cast<float>(cx),
               static_cast<float>(cy - 49), 1.5f);
    text(g, "Every sound starts with an idea.", {cx - 215, cy - 19, 430, 34}, 23, t.text, false,
         juce::Justification::centred);
    text(g, "Your studio starts here.", {cx - 215, cy + 22, 430, 24}, 14, t.muted, false,
         juce::Justification::centred);
    text(g, "Instruments and editing arrive in the next slices.", {cx - 215, cy + 55, 430, 22}, 12, t.muted,
         false, juce::Justification::centred);
    text(g,
         transport.isPlaying() ? "PREVIEW PLAYING  /  Audio not connected" : "READY  /  Audio not connected",
         {24, h - 34, 450, 24}, 11, transport.isPlaying() ? t.accent : t.muted);
    text(g, "C++20  /  JUCE 8  /  0.1.0-dev", {w - 300, h - 34, 275, 24}, 11, t.muted, false,
         juce::Justification::centredRight);
}
void MainComponent::resized()
{
    const int transportX = getWidth() >= 1100 ? 460 : 292;
    play.setBounds(transportX, 26, 38, 38);
    stop.setBounds(transportX + 50, 26, 38, 38);
    tempo.setBounds(transportX + 130, 32, 108, 32);
    themes.setBounds(getWidth() - 202, 30, 178, 36);
}
bool MainComponent::keyPressed(const juce::KeyPress &key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        transport.isPlaying() ? transport.stop() : transport.play();
        updateTransport();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        transport.stop();
        updateTransport();
        return true;
    }
    return false;
}
bool MainComponent::runSmokeChecks()
{
    play.onClick();
    if (!transport.isPlaying() || !play.getToggleState())
        return false;
    stop.onClick();
    tempo.setValue(140, juce::sendNotificationSync);
    if (transport.isPlaying() || transport.tempo() != 140)
        return false;
    tempo.setValue(124, juce::sendNotificationSync);
    themes.setSelectedId(2, juce::sendNotificationSync);
    if (look.tokens.accent != ThemeTokens::ultraviolet().accent)
        return false;
    themes.setSelectedId(1, juce::sendNotificationSync);
    if (look.tokens.accent != ThemeTokens::graphite().accent)
        return false;
    themes.setSelectedId(3, juce::sendNotificationSync);
    if (look.tokens.accent != ThemeTokens::ember().accent)
        return false;
    keyPressed(juce::KeyPress(juce::KeyPress::spaceKey));
    if (!transport.isPlaying())
        return false;
    keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
    for (const auto size : {juce::Point<int>(960, 640), juce::Point<int>(1280, 800)})
    {
        setSize(size.x, size.y);
        for (int i = 0; i < getNumChildComponents(); ++i)
            if (!getLocalBounds().contains(getChildComponent(i)->getBounds()))
                return false;
        if (play.getBounds().intersects(stop.getBounds()) || tempo.getBounds().intersects(themes.getBounds()))
            return false;
    }
    return !transport.isPlaying();
}
} // namespace opendaw
