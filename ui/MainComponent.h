#pragma once
#include "audio/DeviceHost.h"
#include "export/WavExport.h"
#include "persistence/SessionStore.h"
#include "ui/MixerView.h"
#include "ui/StudioView.h"
#include "ui/Theme.h"
#include <thread>

namespace opendaw
{
class MainComponent final : public juce::Component, private juce::Timer
{
  public:
    explicit MainComponent(bool smokeMode = false, bool persistentSession = true);
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
    void showMixer(bool show);
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
    void offerRecovery();
    void savePreferences();
    StudioLookAndFeel look;
    Project project = makeDemoProject();
    Project lastValidProject_ = project;
    AudioEngine engine;
    DeviceHost device{engine};
    RenderState pendingState;
    StudioView studio{project, engine, look.tokens};
    MixerView mixer{project, engine, look.tokens};
    std::vector<Project> undoHistory, redoHistory;
    bool smokeMode_ = false, playing_ = false, pending_ = false, dirty_ = false;
    bool masterDragging_ = false;
    juce::String error_, notice_;
    juce::File projectFile_;
    std::unique_ptr<juce::FileChooser> chooser_;
    bool chooserActive_ = false;
    ExportProgress exportProgress_;
    std::atomic<bool> exporting_{false};
    std::jthread exportThread_;
    std::unique_ptr<juce::DocumentWindow> settingsWindow;
    std::unique_ptr<SessionStore> sessionStore_;
    bool recoveryReady_ = false;
    std::uint32_t lastCheckpoint_ = 0;
    std::uint64_t revision_ = 0, checkpointRevision_ = 0;
    juce::TextButton play{"Play"}, stop{"Stop"};
    juce::TextButton demo{"Load demo"}, clear{"Clear pattern"}, undoButton{"Undo"}, redoButton{"Redo"};
    juce::TextButton loop{"Loop"}, metronome{"Click"}, audioSettings{"Audio settings"};
    juce::TextButton newButton{"New"}, openButton{"Open"}, saveButton{"Save"}, exportButton{"WAV"};
    juce::TextButton mixerButton{"Mixer / F3"};
    juce::Slider tempo, master;
    juce::ComboBox themes, instrument;
    juce::Label projectName;
    juce::TooltipWindow tooltips{this, 600};
};
} // namespace opendaw
