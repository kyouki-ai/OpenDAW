#pragma once
#include "audio/DeviceHost.h"
#include "export/WavExport.h"
#include "ui/StudioView.h"
#include "ui/Theme.h"
#include <thread>

namespace opendaw
{
class MainComponent final : public juce::Component, private juce::Timer
{
  public:
    explicit MainComponent(bool smokeMode = false);
    ~MainComponent() override;
    void paint(juce::Graphics &) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress &) override;
    [[nodiscard]] bool runSmokeChecks();
    [[nodiscard]] bool runDeviceChecks() const;
    void showPianoRoll(bool show)
    {
        studio.showPianoRoll(show);
        timerCallback();
    }
    void requestClose(std::function<void()>);

  private:
    void timerCallback() override;
    void remember();
    void publishProject(bool markDirty = true);
    void undo();
    void redo();
    void showAudioSettings();
    void confirmDiscard(std::function<void()>);
    void save(bool chooseFile, std::function<void(bool)> = {});
    void open();
    void exportAudio();
    void updateTransport();
    StudioLookAndFeel look;
    Project project = makeDemoProject();
    AudioEngine engine;
    DeviceHost device{engine};
    RenderState pendingState;
    StudioView studio{project, engine, look.tokens};
    std::vector<Project> undoHistory, redoHistory;
    bool smokeMode_ = false, playing_ = false, pending_ = false, dirty_ = false;
    juce::String error_, notice_;
    juce::File projectFile_;
    std::unique_ptr<juce::FileChooser> chooser_;
    bool chooserActive_ = false;
    ExportProgress exportProgress_;
    std::atomic<bool> exporting_{false};
    std::jthread exportThread_;
    std::unique_ptr<juce::DocumentWindow> settingsWindow;
    juce::TextButton play{"Play"}, stop{"Stop"};
    juce::TextButton demo{"Load demo"}, clear{"Clear pattern"}, undoButton{"Undo"}, redoButton{"Redo"};
    juce::TextButton loop{"Loop"}, metronome{"Click"}, audioSettings{"Audio settings"};
    juce::TextButton newButton{"New"}, openButton{"Open"}, saveButton{"Save"}, exportButton{"WAV"};
    juce::Slider tempo, master;
    juce::ComboBox themes, instrument;
    juce::TooltipWindow tooltips{this, 600};
};
} // namespace opendaw
