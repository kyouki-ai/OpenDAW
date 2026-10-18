#include "ui/MixerView.h"
#include <cmath>

namespace opendaw
{
class MixerView::Strip final : public juce::Component
{
  public:
    Strip(MixerView &owner, std::size_t index) : owner_(owner), index_(index)
    {
        for (auto *child : {static_cast<juce::Component *>(&gain), static_cast<juce::Component *>(&pan),
                            static_cast<juce::Component *>(&cutoff), static_cast<juce::Component *>(&trim),
                            static_cast<juce::Component *>(&mute), static_cast<juce::Component *>(&solo),
                            static_cast<juce::Component *>(&filter)})
            addAndMakeVisible(child);
        configure(gain, "Channel volume", 0, 1, 0.01, false);
        configure(pan, "Pan: left / centre / right", -1, 1, 0.01, true);
        configure(cutoff, "Low-pass cutoff in Hz", 20, 20000, 1, true);
        cutoff.setSkewFactorFromMidPoint(1800);
        cutoff.setTextValueSuffix(" Hz");
        configure(trim, "Effect gain before low-pass", 0, 2, 0.01, true);
        gain.onValueChange = [this]
        { change([this](Track &t) { t.gain = static_cast<float>(gain.getValue()); }); };
        pan.onValueChange = [this]
        { change([this](Track &t) { t.pan = static_cast<float>(pan.getValue()); }); };
        cutoff.onValueChange = [this]
        { change([this](Track &t) { t.filterCutoff = static_cast<float>(cutoff.getValue()); }); };
        trim.onValueChange = [this]
        { change([this](Track &t) { t.effectGain = static_cast<float>(trim.getValue()); }); };
        for (auto *button : {&mute, &solo, &filter})
            button->setClickingTogglesState(true);
        mute.onClick = [this] { change([this](Track &t) { t.mute = mute.getToggleState(); }); };
        solo.onClick = [this] { change([this](Track &t) { t.solo = solo.getToggleState(); }); };
        filter.onClick = [this] { change([this](Track &t) { t.filterEnabled = filter.getToggleState(); }); };
        refresh();
    }
    void configure(juce::Slider &slider, const juce::String &name, double low, double high, double step,
                   bool rotary)
    {
        slider.setRange(low, high, step);
        slider.setSliderStyle(rotary ? juce::Slider::RotaryHorizontalVerticalDrag
                                     : juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 94, 21);
        slider.setTitle(name);
        slider.setTooltip(name);
        slider.onDragStart = [this]
        {
            dragging_ = true;
            if (owner_.beforeEdit)
                owner_.beforeEdit();
        };
        slider.onDragEnd = [this] { dragging_ = false; };
    }
    template <typename Function> void change(Function fn)
    {
        if (index_ >= owner_.project_.tracks.size())
            return;
        if (!dragging_ && owner_.beforeEdit)
            owner_.beforeEdit();
        fn(owner_.project_.tracks[index_]);
        if (owner_.onEdit)
            owner_.onEdit();
        repaint();
    }
    void refresh()
    {
        if (index_ >= owner_.project_.tracks.size())
            return;
        const auto &t = owner_.project_.tracks[index_];
        if (!dragging_)
        {
            gain.setValue(t.gain, juce::dontSendNotification);
            pan.setValue(t.pan, juce::dontSendNotification);
            cutoff.setValue(t.filterCutoff, juce::dontSendNotification);
            trim.setValue(t.effectGain, juce::dontSendNotification);
        }
        mute.setToggleState(t.mute, juce::dontSendNotification);
        solo.setToggleState(t.solo, juce::dontSendNotification);
        filter.setToggleState(t.filterEnabled, juce::dontSendNotification);
        cutoff.setEnabled(t.filterEnabled);
        repaint();
    }
    void resized() override
    {
        const int w = getWidth(), h = getHeight();
        pan.setBounds(12, 63, w - 24, 73);
        filter.setBounds(18, 155, w - 36, 27);
        cutoff.setBounds(12, 184, w - 24, 78);
        trim.setBounds(12, 287, w - 24, 74);
        mute.setBounds(17, h - 43, 50, 27);
        solo.setBounds(w - 67, h - 43, 50, 27);
        gain.setBounds(12, 388, w - 60, std::max(92, h - 444));
    }
    void paint(juce::Graphics &g) override
    {
        if (index_ >= owner_.project_.tracks.size())
            return;
        const auto &t = owner_.theme_;
        const auto &track = owner_.project_.tracks[index_];
        g.setColour(t.raised.withAlpha(0.45f));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(3), 7);
        g.setColour(index_ % 2 == 0 ? t.accent : t.secondary);
        g.fillRoundedRectangle(16, 12, static_cast<float>(getWidth() - 32), 3, 1);
        g.setColour(t.text);
        g.setFont(juce::FontOptions(14, juce::Font::bold));
        g.drawFittedText(juce::String(track.name), 16, 23, getWidth() - 32, 23,
                         juce::Justification::centredLeft, 1);
        g.setColour(t.muted);
        g.setFont(juce::FontOptions(10));
        g.drawText("PAN", 16, 49, 110, 17, juce::Justification::centred);
        g.drawText("01 / LOW-PASS", 16, 136, 110, 17, juce::Justification::centred);
        g.drawText("INPUT GAIN", 8, 267, getWidth() - 16, 17, juce::Justification::centred);
        g.drawText("VOLUME", 16, 369, 110, 17, juce::Justification::centred);
        const float peak = owner_.engine_.meters.channelPeak[index_].load();
        const float meterHeight = static_cast<float>(std::max(65, getHeight() - 484));
        const float x = static_cast<float>(getWidth() - 33);
        g.setColour(t.background);
        g.fillRoundedRectangle(x, 399, 8, meterHeight, 2);
        g.setColour(peak > 0.95f ? t.accent : t.secondary);
        const float level = juce::jlimit(0.0f, 1.0f, peak * 2);
        g.fillRoundedRectangle(x, 399 + meterHeight * (1 - level), 8, meterHeight * level, 2);
    }
    juce::Slider gain, pan, cutoff, trim;
    juce::TextButton mute{"M"}, solo{"S"}, filter{"Low-pass"};

  private:
    MixerView &owner_;
    std::size_t index_;
    bool dragging_ = false;
};
MixerView::MixerView(Project &project, AudioEngine &engine, const ThemeTokens &theme)
    : project_(project), engine_(engine), theme_(theme)
{
    addAndMakeVisible(viewport_);
    viewport_.setViewedComponent(&content_, false);
    viewport_.setScrollBarsShown(true, true);
    refresh();
}
MixerView::~MixerView()
{
    viewport_.setViewedComponent(nullptr, false);
}
void MixerView::refresh()
{
    if (strips_.size() != project_.tracks.size())
    {
        strips_.clear();
        for (std::size_t i = 0; i < project_.tracks.size(); ++i)
        {
            auto strip = std::make_unique<Strip>(*this, i);
            content_.addAndMakeVisible(*strip);
            strips_.push_back(std::move(strip));
        }
        resized();
    }
    for (auto &strip : strips_)
        strip->refresh();
}
void MixerView::resized()
{
    viewport_.setBounds(8, 55, std::max(1, getWidth() - 16), std::max(1, getHeight() - 63));
    const int width = std::max(viewport_.getWidth() - 15, static_cast<int>(strips_.size()) * 148);
    content_.setSize(width, std::max(595, viewport_.getHeight() - 16));
    for (std::size_t i = 0; i < strips_.size(); ++i)
        strips_[i]->setBounds(static_cast<int>(i) * width / static_cast<int>(strips_.size()), 0,
                              width / static_cast<int>(strips_.size()), content_.getHeight());
}
void MixerView::paint(juce::Graphics &g)
{
    g.setColour(theme_.surface);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8);
    g.setColour(theme_.border);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8, 1);
    g.setColour(theme_.text);
    g.setFont(juce::FontOptions(17, juce::Font::bold));
    g.drawText("Mixer", 18, 10, 160, 30, juce::Justification::centredLeft);
    g.setColour(theme_.muted);
    g.setFont(juce::FontOptions(12));
    g.drawText("Channels > Master    /    F3: back to arrangement", 160, 10, getWidth() - 178, 30,
               juce::Justification::centredRight);
}
bool MixerView::exerciseEditing()
{
    if (strips_.empty())
        return false;
    const auto saved = project_;
    strips_[0]->gain.setValue(0.42, juce::sendNotificationSync);
    strips_[0]->filter.setToggleState(true, juce::dontSendNotification);
    strips_[0]->filter.onClick();
    strips_[0]->cutoff.setValue(1450, juce::sendNotificationSync);
    const bool ok = std::abs(project_.tracks[0].gain - 0.42f) < 0.001f && project_.tracks[0].filterEnabled &&
                    project_.tracks[0].filterCutoff == 1450 && validateProject(project_).empty();
    project_ = saved;
    if (onEdit)
        onEdit();
    refresh();
    return ok;
}
} // namespace opendaw
