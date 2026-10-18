#pragma once
#include "audio/AudioEngine.h"
#include "ui/Theme.h"
#include <functional>

namespace opendaw
{
class StudioView final : public juce::Component, public juce::TooltipClient
{
  public:
    StudioView(Project &project, AudioEngine &engine, const ThemeTokens &theme);
    void paint(juce::Graphics &) override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &) override;
    bool keyPressed(const juce::KeyPress &) override;
    juce::String getTooltip() override;
    std::function<void()> beforeEdit, onEdit;
    [[nodiscard]] int selectedTrack() const
    {
        return selectedTrack_;
    }
    [[nodiscard]] Pattern *selectedPattern();
    void resetSelection();
    void showPianoRoll(bool show)
    {
        piano_ = show;
        if (show && project_.tracks.size() > 3)
        {
            selectedTrack_ = 3;
            topPitch_ = 54;
        }
        repaint();
    }
    bool exerciseEditing(); // GUI smoke: real editing handlers, restored afterwards
  private:
    juce::Rectangle<int> arrangement() const;
    juce::Rectangle<int> editor() const;
    juce::Rectangle<int> pianoGrid() const;
    juce::Rectangle<int> stepCell(int channel, int step) const;
    juce::Rectangle<float> noteBounds(const Note &) const;
    int noteAt(juce::Point<float>) const;
    Tick tickAt(float x) const;
    int pitchAt(float y) const;
    void changed();
    void beginEdit();
    void toggleStep(int channel, int step);
    void duplicateNotes(bool fromClipboard = false);
    Pattern *patternForTrack(int track);
    Project &project_;
    AudioEngine &engine_;
    const ThemeTokens &theme_;
    int selectedTrack_ = 0, topPitch_ = 72;
    bool piano_ = false, editStarted_ = false, resizeNote_ = false, velocityDrag_ = false;
    int dragNote_ = -1, dragClip_ = -1, clipTrack_ = -1;
    bool resizeClip_ = false, gainDrag_ = false, loopDrag_ = false;
    Tick originalStart_ = 0, originalDuration_ = 0, dragTimeline_ = ppq * 32;
    int originalPitch_ = 60;
    juce::Point<float> dragOrigin_;
    std::vector<std::string> selectedNotes_;
    std::vector<Note> clipboard_;
};
} // namespace opendaw
