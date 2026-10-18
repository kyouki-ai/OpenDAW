#include "ui/MainComponent.h"
#include "persistence/ProjectStore.h"
#include "ui/AudioSettings.h"

namespace opendaw
{
namespace
{
juce::String sessionFileName(const Project &project)
{
    const auto name = juce::File::createLegalFileName(juce::String(project.name));
    return name.isEmpty() ? "Untitled" : name;
}
void confirmDestination(juce::Component *owner, const juce::File &chosen, const juce::String &extension,
                        std::function<void(juce::File)> complete)
{
    const auto target = chosen.hasFileExtension(extension) ? chosen : chosen.withFileExtension(extension);
    // The native dialog checked the chosen name, which may differ after extension normalization.
    if (target != chosen && target.existsAsFile())
        juce::AlertWindow::showOkCancelBox(
            juce::MessageBoxIconType::QuestionIcon, "Replace existing file?",
            target.getFileName() + " already exists. Replace it?", "Replace", "Cancel", owner,
            juce::ModalCallbackFunction::create([target, complete = std::move(complete)](int answer)
                                                { complete(answer == 1 ? target : juce::File{}); }));
    else
        complete(target);
}
void text(juce::Graphics &g, const juce::String &value, juce::Rectangle<int> area, float size,
          juce::Colour colour, bool bold = false)
{
    g.setColour(colour);
    g.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
    g.drawText(value, area, juce::Justification::centredLeft);
}
class SettingsWindow final : public juce::DocumentWindow
{
  public:
    SettingsWindow(DeviceHost &device, StudioLookAndFeel &look)
        : DocumentWindow("Audio settings", look.tokens.background, closeButton)
    {
        setLookAndFeel(&look);
        setUsingNativeTitleBar(true);
        setContentOwned(new AudioSettings(device, look.tokens), true);
        centreWithSize(520, 350);
        setVisible(true);
    }
    ~SettingsWindow() override
    {
        setLookAndFeel(nullptr);
    }
    void closeButtonPressed() override
    {
        setVisible(false);
    }
};
} // namespace
MainComponent::MainComponent(bool smokeMode, bool persistentSession) : smokeMode_(smokeMode)
{
    setLookAndFeel(&look);
    setWantsKeyboardFocus(true);
    for (auto *child :
         {static_cast<juce::Component *>(&play),          static_cast<juce::Component *>(&stop),
          static_cast<juce::Component *>(&tempo),         static_cast<juce::Component *>(&themes),
          static_cast<juce::Component *>(&studio),        static_cast<juce::Component *>(&master),
          static_cast<juce::Component *>(&mixer),         static_cast<juce::Component *>(&mixerButton),
          static_cast<juce::Component *>(&demo),          static_cast<juce::Component *>(&clear),
          static_cast<juce::Component *>(&undoButton),    static_cast<juce::Component *>(&redoButton),
          static_cast<juce::Component *>(&loop),          static_cast<juce::Component *>(&metronome),
          static_cast<juce::Component *>(&audioSettings), static_cast<juce::Component *>(&newButton),
          static_cast<juce::Component *>(&openButton),    static_cast<juce::Component *>(&saveButton),
          static_cast<juce::Component *>(&exportButton),  static_cast<juce::Component *>(&instrument),
          static_cast<juce::Component *>(&projectName)})
        addAndMakeVisible(child);
    projectName.setText(juce::String(project.name), juce::dontSendNotification);
    projectName.setFont(juce::FontOptions(15, juce::Font::bold));
    projectName.setBorderSize({0, 2, 0, 2});
    projectName.setColour(juce::Label::textColourId, look.tokens.text);
    projectName.setTitle("Session name");
    projectName.setTooltip("Double-click or press F2 to rename your session");
    projectName.setEditable(false, true, false);
    projectName.onTextChange = [this]
    {
        const auto name = projectName.getText().trim();
        if (name.isEmpty() || name.length() > 64)
        {
            error_ = "Choose a session name with 1 to 64 characters.";
            projectName.setText(juce::String(project.name), juce::dontSendNotification);
            repaint();
            return;
        }
        if (name.toStdString() != project.name)
        {
            remember();
            project.name = name.toStdString();
            publishProject();
        }
    };
    play.setComponentID("transport-play");
    stop.setComponentID("transport-stop");
    play.setTitle("Play or stop");
    stop.setTitle("Stop and return to start");
    play.setTooltip("Play / stop - Space");
    stop.setTooltip("Stop and return to start - Escape");
    play.onClick = [this]
    {
        playing_ = !playing_;
        playing_ ? engine.play() : engine.stop();
        updateTransport();
    };
    stop.onClick = [this]
    {
        playing_ = false;
        engine.stop();
        updateTransport();
    };
    tempo.setSliderStyle(juce::Slider::IncDecButtons);
    tempo.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 65, 32);
    tempo.setRange(20, 300, 1);
    tempo.setValue(project.bpm, juce::dontSendNotification);
    tempo.setTitle("Tempo in beats per minute");
    tempo.onValueChange = [this]
    {
        remember();
        project.bpm = tempo.getValue();
        publishProject();
    };
    master.setSliderStyle(juce::Slider::LinearVertical);
    master.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    master.setRange(0, 1, 0.01);
    master.setValue(project.masterGain, juce::dontSendNotification);
    master.setTitle("Master volume");
    master.setTooltip("Master output level");
    master.onDragStart = [this]
    {
        masterDragging_ = true;
        remember();
    };
    master.onDragEnd = [this] { masterDragging_ = false; };
    master.onValueChange = [this]
    {
        if (!masterDragging_)
            remember();
        project.masterGain = static_cast<float>(master.getValue());
        publishProject();
    };
    themes.addItem("Graphite / Mint", 1);
    themes.addItem("Ultraviolet / Ice", 2);
    themes.addItem("Ember / Sand", 3);
    themes.setSelectedId(3, juce::dontSendNotification);
    themes.setTitle("Theme");
    themes.onChange = [this]
    {
        look.setTheme(themes.getSelectedId() == 2   ? ThemeTokens::ultraviolet()
                      : themes.getSelectedId() == 3 ? ThemeTokens::ember()
                                                    : ThemeTokens::graphite());
        sendLookAndFeelChange();
        projectName.setColour(juce::Label::textColourId, look.tokens.text);
        studio.repaint();
        repaint();
    };
    studio.beforeEdit = [this] { remember(); };
    studio.onEdit = [this] { publishProject(); };
    studio.onError = [this](const juce::String &message)
    {
        error_ = message;
        repaint();
    };
    mixer.beforeEdit = [this] { remember(); };
    mixer.onEdit = [this] { publishProject(); };
    mixer.setVisible(false);
    mixerButton.onClick = [this] { showMixer(!mixer.isVisible()); };
    demo.onClick = [this]
    {
        remember();
        engine.stop();
        playing_ = false;
        project = makeDemoProject();
        studio.resetSelection();
        publishProject();
        updateTransport();
    };
    clear.onClick = [this]
    {
        if (auto *pattern = studio.selectedPattern())
        {
            remember();
            pattern->notes.clear();
            publishProject();
        }
    };
    undoButton.onClick = [this] { undo(); };
    redoButton.onClick = [this] { redo(); };
    loop.setClickingTogglesState(true);
    metronome.setClickingTogglesState(true);
    loop.onClick = [this]
    {
        remember();
        project.loopEnabled = loop.getToggleState();
        publishProject();
    };
    metronome.onClick = [this]
    {
        remember();
        project.metronome = metronome.getToggleState();
        publishProject();
    };
    audioSettings.onClick = [this] { showAudioSettings(); };
    demo.setButtonText("Demo");
    clear.setButtonText("Clear");
    clear.setTooltip("Clear notes in the selected pattern. Ctrl+Z restores them.");
    metronome.setTooltip("Metronome - excluded from WAV export");
    saveButton.setTooltip("Save project - Ctrl+S. Save as - Ctrl+Shift+S");
    exportButton.setTooltip("Export stereo 24-bit / 48-kHz WAV - Ctrl+E");
    newButton.onClick = [this]
    {
        confirmDiscard(
            [this]
            {
                engine.stop();
                playing_ = false;
                project = makeDemoProject();
                project.name = "Untitled session";
                for (auto &pattern : project.patterns)
                    pattern.notes.clear();
                projectFile_ = {};
                if (sessionStore_ && recoveryReady_)
                    (void)sessionStore_->clearRecovery();
                dirty_ = false;
                undoHistory.clear();
                redoHistory.clear();
                studio.resetSelection();
                publishProject(false);
                updateTransport();
            });
    };
    openButton.onClick = [this] { open(); };
    saveButton.onClick = [this] { save(false); };
    exportButton.onClick = [this] { exportAudio(); };
    instrument.addItemList({"Kick", "Snare", "Closed hat", "Sine synth", "Triangle bass"}, 1);
    instrument.setSelectedId(static_cast<int>(project.tracks[0].instrument) + 1, juce::dontSendNotification);
    instrument.setTitle("Track instrument");
    instrument.onChange = [this]
    {
        const int selected = studio.selectedTrack();
        if (selected < static_cast<int>(project.tracks.size()))
        {
            remember();
            project.tracks[static_cast<std::size_t>(selected)].instrument =
                static_cast<Track::Instrument>(instrument.getSelectedId() - 1);
            publishProject();
        }
    };
    Preferences preferences;
    if (!smokeMode_ && persistentSession)
    {
        sessionStore_ = std::make_unique<SessionStore>(
            juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("OpenDAW"));
        preferences = sessionStore_->preferences();
        themes.setSelectedId(preferences.theme, juce::sendNotificationSync);
    }
    setSize(preferences.width, preferences.height);
    publishProject(false);
    if (!smokeMode_)
    {
        const auto savedDevice = juce::parseXML(preferences.deviceXml);
        error_ = device.start(savedDevice.get());
    }
    else
        engine.prepare(48000);
    updateTransport();
    lastCheckpoint_ = juce::Time::getMillisecondCounter();
    if (sessionStore_)
        juce::Timer::callAfterDelay(100,
                                    [safe = juce::Component::SafePointer<MainComponent>(this)]
                                    {
                                        if (safe)
                                            safe->offerRecovery();
                                    });
    startTimerHz(30);
}
MainComponent::~MainComponent()
{
    stopTimer();
    exportProgress_.cancel.store(true);
    if (exportThread_.joinable())
        exportThread_.join();
    chooser_.reset();
    settingsWindow.reset();
    device.shutdown();
    setLookAndFeel(nullptr);
}
void MainComponent::remember()
{
    if (undoHistory.size() >= 64)
        undoHistory.erase(undoHistory.begin());
    undoHistory.push_back(project);
    redoHistory.clear();
}
void MainComponent::publishProject(bool markDirty)
{
    notice_.clear();
    error_ = juce::String(compileProject(project, pendingState));
    if (error_.isNotEmpty())
    {
        if (markDirty)
        {
            project = lastValidProject_;
            studio.resetSelection();
            if (compileProject(project, pendingState).empty())
                pending_ = !engine.publish(pendingState);
        }
        repaint();
        return;
    }
    pending_ = !engine.publish(pendingState);
    lastValidProject_ = project;
    projectName.setText(juce::String(project.name), juce::dontSendNotification);
    dirty_ = dirty_ || markDirty;
    if (markDirty)
        ++revision_;
    tempo.setValue(project.bpm, juce::dontSendNotification);
    master.setValue(project.masterGain, juce::dontSendNotification);
    loop.setToggleState(project.loopEnabled, juce::dontSendNotification);
    metronome.setToggleState(project.metronome, juce::dontSendNotification);
    undoButton.setEnabled(!undoHistory.empty());
    redoButton.setEnabled(!redoHistory.empty());
    instrument.setEnabled(!project.tracks.empty());
    studio.repaint();
    mixer.refresh();
    repaint();
}
void MainComponent::undo()
{
    if (undoHistory.empty())
        return;
    redoHistory.push_back(project);
    project = undoHistory.back();
    undoHistory.pop_back();
    studio.resetSelection();
    publishProject();
}
void MainComponent::redo()
{
    if (redoHistory.empty())
        return;
    undoHistory.push_back(project);
    project = redoHistory.back();
    redoHistory.pop_back();
    studio.resetSelection();
    publishProject();
}
void MainComponent::showAudioSettings()
{
    if (smokeMode_)
        return;
    if (!settingsWindow)
        settingsWindow = std::make_unique<SettingsWindow>(device, look);
    settingsWindow->setVisible(true);
    settingsWindow->toFront(true);
}
void MainComponent::showMixer(bool show)
{
    mixer.setVisible(show);
    studio.setVisible(!show);
    mixerButton.setToggleState(show, juce::dontSendNotification);
    if (show)
        mixer.refresh();
}
void MainComponent::confirmDiscard(std::function<void()> proceed)
{
    if (!dirty_)
    {
        proceed();
        return;
    }
    const juce::Component::SafePointer<MainComponent> safe(this);
    juce::AlertWindow::showYesNoCancelBox(juce::MessageBoxIconType::QuestionIcon, "Save your session?",
                                          "Save your changes before continuing?", "Save", "Discard", "Cancel",
                                          this,
                                          juce::ModalCallbackFunction::create(
                                              [safe, proceed = std::move(proceed)](int result)
                                              {
                                                  if (!safe)
                                                      return;
                                                  if (result == 1)
                                                      safe->save(false,
                                                                 [safe, proceed](bool saved)
                                                                 {
                                                                     if (safe && saved)
                                                                         proceed();
                                                                 });
                                                  else if (result == 2)
                                                      proceed();
                                              }));
}
void MainComponent::requestClose(std::function<void()> finish)
{
    if (smokeMode_)
    {
        finish();
        return;
    }
    confirmDiscard(
        [this, finish = std::move(finish)]
        {
            savePreferences();
            if (sessionStore_ && recoveryReady_)
                (void)sessionStore_->clearRecovery();
            finish();
        });
}
void MainComponent::savePreferences()
{
    if (!sessionStore_)
        return;
    Preferences p;
    p.theme = themes.getSelectedId();
    p.width = juce::jlimit(960, 3840, getWidth());
    p.height = juce::jlimit(640, 2160, getHeight());
    if (const auto xml = device.manager.createStateXml())
        p.deviceXml = xml->toString();
    if (const auto result = sessionStore_->savePreferences(p); result.failed())
        error_ = result.getErrorMessage();
}
void MainComponent::offerRecovery()
{
    if (!sessionStore_)
        return;
    if (!sessionStore_->hasRecovery())
    {
        recoveryReady_ = true;
        return;
    }
    Project recovered;
    auto result = sessionStore_->loadRecovery(recovered);
    if (result.wasOk())
    {
        auto state = std::make_unique<RenderState>();
        if (const auto error = compileProject(recovered, *state); !error.empty())
            result = juce::Result::fail(error);
    }
    if (result.failed())
    {
        error_ = "Recovery could not be read. Original kept; automatic checkpoints paused.";
        repaint();
        return;
    }
    const juce::Component::SafePointer<MainComponent> safe(this);
    juce::AlertWindow::showYesNoCancelBox(
        juce::MessageBoxIconType::QuestionIcon, "Recover your session?",
        "An unfinished session was found. Restore your notes and arrangement?", "Recover", "Discard", "Exit",
        this,
        juce::ModalCallbackFunction::create(
            [safe, recovered = std::move(recovered)](int answer) mutable
            {
                if (!safe)
                    return;
                if (answer == 1)
                {
                    safe->engine.stop();
                    safe->playing_ = false;
                    safe->project = std::move(recovered);
                    safe->projectFile_ = {};
                    safe->undoHistory.clear();
                    safe->redoHistory.clear();
                    safe->studio.resetSelection();
                    safe->recoveryReady_ = true;
                    safe->publishProject();
                    safe->notice_ = "Recovered session. Save it with Ctrl+S.";
                    safe->updateTransport();
                }
                else if (answer == 2)
                {
                    safe->recoveryReady_ = safe->sessionStore_->clearRecovery();
                    if (!safe->recoveryReady_)
                        safe->error_ = "Could not discard recovery; original kept.";
                    safe->repaint();
                }
                else
                    juce::JUCEApplication::getInstance()->quit();
            }));
}
void MainComponent::save(bool chooseFile, std::function<void(bool)> complete)
{
    if (chooserActive_)
    {
        if (complete)
            complete(false);
        return;
    }
    if (!chooseFile && projectFile_ != juce::File{})
    {
        const auto result = saveProject(project, projectFile_);
        error_ = result.getErrorMessage();
        if (result.wasOk())
        {
            dirty_ = false;
            if (sessionStore_ && recoveryReady_)
                (void)sessionStore_->clearRecovery();
            notice_ = "Saved " + projectFile_.getFileName();
        }
        repaint();
        if (complete)
            complete(result.wasOk());
        return;
    }
    chooser_ = std::make_unique<juce::FileChooser>(
        "Save OpenDAW project",
        projectFile_ == juce::File{} ? juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                           .getChildFile(sessionFileName(project) + ".opendaw")
                                     : projectFile_,
        "*.opendaw");
    chooserActive_ = true;
    const juce::Component::SafePointer<MainComponent> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, complete](const juce::FileChooser &chooser)
                          {
                              if (!safe)
                                  return;
                              safe->chooserActive_ = false;
                              auto file = chooser.getResult();
                              if (file == juce::File{})
                              {
                                  if (complete)
                                      complete(false);
                                  return;
                              }
                              confirmDestination(safe.getComponent(), file, "opendaw",
                                                 [safe, complete](juce::File file)
                                                 {
                                                     if (!safe)
                                                         return;
                                                     if (file == juce::File{})
                                                     {
                                                         if (complete)
                                                             complete(false);
                                                         return;
                                                     }
                                                     const auto result = saveProject(safe->project, file);
                                                     safe->error_ = result.getErrorMessage();
                                                     if (result.wasOk())
                                                     {
                                                         safe->projectFile_ = file;
                                                         safe->dirty_ = false;
                                                         if (safe->sessionStore_ && safe->recoveryReady_)
                                                             (void)safe->sessionStore_->clearRecovery();
                                                         safe->notice_ = "Saved " + file.getFileName();
                                                     }
                                                     safe->repaint();
                                                     if (complete)
                                                         complete(result.wasOk());
                                                 });
                          });
}
void MainComponent::open()
{
    if (chooserActive_)
        return;
    confirmDiscard(
        [safe = juce::Component::SafePointer<MainComponent>(this)]
        {
            if (!safe)
                return;
            safe->chooser_ =
                std::make_unique<juce::FileChooser>("Open OpenDAW project", safe->projectFile_, "*.opendaw");
            safe->chooserActive_ = true;
            safe->chooser_->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safe](const juce::FileChooser &chooser)
                {
                    if (!safe)
                        return;
                    safe->chooserActive_ = false;
                    const auto file = chooser.getResult();
                    if (file == juce::File{})
                        return;
                    Project loaded;
                    const auto result = loadProject(file, loaded);
                    if (result.failed())
                    {
                        safe->error_ = result.getErrorMessage();
                        safe->repaint();
                        return;
                    }
                    if (const auto error = compileProject(loaded, safe->pendingState); !error.empty())
                    {
                        safe->error_ = juce::String(error);
                        safe->repaint();
                        return;
                    }
                    safe->engine.stop();
                    safe->playing_ = false;
                    safe->project = std::move(loaded);
                    safe->projectFile_ = file;
                    if (safe->sessionStore_ && safe->recoveryReady_)
                        (void)safe->sessionStore_->clearRecovery();
                    safe->dirty_ = false;
                    safe->undoHistory.clear();
                    safe->redoHistory.clear();
                    safe->studio.resetSelection();
                    safe->publishProject(false);
                    safe->updateTransport();
                });
        });
}
void MainComponent::exportAudio()
{
    if (chooserActive_ || exporting_.load())
        return;
    chooser_ =
        std::make_unique<juce::FileChooser>("Export stereo WAV (24-bit / 48 kHz)",
                                            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                .getChildFile(sessionFileName(project) + ".wav"),
                                            "*.wav");
    chooserActive_ = true;
    const juce::Component::SafePointer<MainComponent> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe](const juce::FileChooser &chooser)
                          {
                              if (!safe)
                                  return;
                              safe->chooserActive_ = false;
                              auto file = chooser.getResult();
                              if (file == juce::File{})
                                  return;
                              confirmDestination(
                                  safe.getComponent(), file, "wav",
                                  [safe](juce::File file)
                                  {
                                      if (!safe || file == juce::File{} || safe->exporting_.load())
                                          return;
                                      safe->exportProgress_.cancel.store(false);
                                      safe->exportProgress_.fraction.store(0);
                                      safe->exporting_.store(true);
                                      safe->exportButton.setEnabled(false);
#if JUCE_WINDOWS
                                      const std::filesystem::path path(
                                          std::wstring(file.getFullPathName().toWideCharPointer()));
#else
            const auto path = std::filesystem::u8path(file.getFullPathName().toStdString());
#endif
                                      safe->exportThread_ = std::jthread(
                                          [safe, owner = safe.getComponent(), snapshot = safe->project, path,
                                           name = file.getFileName()]
                                          {
                                              WavResult result;
                                              try
                                              {
                                                  result = exportWav(snapshot, path, 48000, 24,
                                                                     &owner->exportProgress_);
                                              }
                                              catch (const std::exception &error)
                                              {
                                                  result.error = error.what();
                                              }
                                              owner->exporting_.store(false);
                                              juce::MessageManager::callAsync(
                                                  [safe, result, name]
                                                  {
                                                      if (!safe)
                                                          return;
                                                      safe->exportButton.setEnabled(true);
                                                      safe->error_ = juce::String(result.error);
                                                      if (result.error.empty())
                                                          safe->notice_ = "Exported " + name;
                                                      safe->repaint();
                                                  });
                                          });
                                  });
                          });
}
void MainComponent::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    if (sessionStore_ && recoveryReady_ && now - lastCheckpoint_ >= 30000)
    {
        lastCheckpoint_ = now;
        if (dirty_ && revision_ != checkpointRevision_)
        {
            const auto result = sessionStore_->checkpoint(project);
            if (result.wasOk())
                checkpointRevision_ = revision_;
            else
                error_ = "Automatic checkpoint failed: " + result.getErrorMessage();
        }
    }
    const int selectedTrack = studio.selectedTrack();
    if (selectedTrack < static_cast<int>(project.tracks.size()))
        instrument.setSelectedId(
            static_cast<int>(project.tracks[static_cast<std::size_t>(selectedTrack)].instrument) + 1,
            juce::dontSendNotification);
    if (exporting_.load())
    {
        notice_ =
            "Exporting WAV " + juce::String(static_cast<int>(exportProgress_.fraction.load() * 100)) + "%";
        repaint();
    }
    if (pending_)
        pending_ = !engine.publish(pendingState);
    if (!smokeMode_ && device.available())
    {
        const bool playing = engine.meters.playing.load(std::memory_order_relaxed);
        if (playing_ != playing)
        {
            playing_ = playing;
            updateTransport();
        }
    }
    if (playing_ || engine.meters.masterPeak.load(std::memory_order_relaxed) > 0.001f)
    {
        studio.repaint();
        if (mixer.isVisible())
            mixer.refresh();
        repaint();
    }
}
void MainComponent::updateTransport()
{
    play.setToggleState(playing_, juce::dontSendNotification);
    repaint();
}
void MainComponent::paint(juce::Graphics &g)
{
    const auto &t = look.tokens;
    const int w = getWidth(), h = getHeight(), tx = w >= 1100 ? 400 : 292;
    g.fillAll(t.background);
    g.setGradientFill(juce::ColourGradient(t.surface.brighter(0.025f), 0, 0, t.background, 0, 88, false));
    g.fillRect(0, 0, w, 88);
    g.setColour(t.border);
    g.drawHorizontalLine(87, 0, static_cast<float>(w));
    juce::Path wave;
    wave.startNewSubPath(25, 43);
    wave.cubicTo(31, 43, 29, 31, 34, 31);
    wave.cubicTo(39, 31, 37, 55, 43, 55);
    wave.cubicTo(49, 55, 47, 22, 53, 22);
    wave.cubicTo(59, 22, 57, 55, 63, 55);
    wave.cubicTo(69, 55, 67, 34, 72, 34);
    wave.cubicTo(76, 34, 75, 43, 79, 43);
    g.setColour(t.accent);
    g.strokePath(wave,
                 juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    text(g, "OpenDAW", {92, 24, 118, 28}, 22, t.text);
    text(g, "MAKE SOMETHING YOURS", {93, 52, 155, 16}, 9, t.muted);
    text(g, "BPM", {tx + 130, 12, 100, 18}, 10, t.muted);
    const auto tick = engine.meters.positionTicks.load(std::memory_order_relaxed);
    const Tick barLength = ppq * project.numerator * 4 / project.denominator;
    const auto position = juce::String(tick / barLength + 1).paddedLeft('0', 3) + " : " +
                          juce::String(tick % barLength / ppq + 1).paddedLeft('0', 2) + " : " +
                          juce::String(tick % ppq).paddedLeft('0', 3);
    text(g, position, {tx + 270, 27, 155, 30}, 20, t.text);
    text(g, "BAR      BEAT      TICK", {tx + 270, 56, 155, 16}, 9, t.muted);
    if (w >= 1100)
    {
        g.setColour(t.border);
        g.drawVerticalLine(226, 22, 62);
        text(g, juce::String(project.name) + (dirty_ ? " *" : ""), {250, 30, 140, 26}, 15, t.text);
        text(g, juce::String(project.numerator) + " / " + juce::String(project.denominator),
             {tx + 450, 32, 60, 20}, 13, t.muted);
    }
    g.setColour(t.surface);
    g.fillRoundedRectangle(16, 104, 160, static_cast<float>(h - 152), 8);
    text(g, "YOUR SESSION", {30, 120, 132, 22}, 10, t.muted, true);
    text(g, "PLAYBACK", {30, 366, 132, 20}, 10, t.muted, true);
    text(g, "INSTRUMENT", {30, 432, 132, 20}, 10, t.muted, true);
    if (h >= 720)
    {
        text(g, "1. Press Play", {30, 530, 132, 24}, 12, t.text);
        text(g, "2. Click a few steps", {30, 557, 132, 24}, 12, t.text);
        text(g, "3. Make it your own", {30, 584, 132, 24}, 12, t.text);
    }
    const int mx = w - 88;
    g.setColour(t.surface);
    g.fillRoundedRectangle(static_cast<float>(mx - 8), 104, 80, static_cast<float>(h - 152), 8);
    text(g, "MASTER", {mx, 118, 70, 22}, 11, t.muted, true);
    const float mh = static_cast<float>(std::max(70, (h - 240) / 2));
    for (int channel = 0; channel < 2; ++channel)
    {
        const float peak = juce::jlimit(
            0.0f, 1.0f, channel == 0 ? engine.meters.masterLeft.load() : engine.meters.masterRight.load());
        const float x = static_cast<float>(mx + 12 + channel * 20);
        g.setColour(t.background);
        g.fillRoundedRectangle(x, 158, 9, mh, 2);
        g.setGradientFill(juce::ColourGradient(t.accent, x, 158, t.secondary, x, 158 + mh, false));
        g.fillRoundedRectangle(x, 158 + mh * (1 - peak), 9, mh * peak, 2);
    }
    text(g, "LEVEL", {mx + 7, h - 100, 63, 22}, 10, t.muted);
    const juce::String status = error_.isNotEmpty()    ? error_
                                : notice_.isNotEmpty() ? notice_
                                : smokeMode_           ? "Offline render - audio output not opened"
                                                       : device.description();
    text(g, status, {24, h - 34, std::max(200, w - 340), 24}, 11, error_.isEmpty() ? t.muted : t.accent);
    const auto cpu = engine.meters.cpuLoad.load() * 100;
    text(g, "CPU " + juce::String(cpu, 1) + "%  /  " + juce::String(device.manager.getXRunCount()) + " xruns",
         {w - 280, h - 34, 260, 24}, 11, t.muted);
}
void MainComponent::resized()
{
    const int tx = getWidth() >= 1100 ? 400 : 292;
    play.setBounds(tx, 26, 38, 38);
    stop.setBounds(tx + 50, 26, 38, 38);
    tempo.setBounds(tx + 130, 32, 108, 32);
    themes.setBounds(getWidth() - 202, 30, 178, 36);
    studio.setBounds(190, 104, getWidth() - 298, getHeight() - 152);
    mixer.setBounds(studio.getBounds());
    newButton.setBounds(28, 192, 64, 32);
    projectName.setBounds(28, 151, 136, 26);
    openButton.setBounds(100, 192, 64, 32);
    saveButton.setBounds(28, 234, 64, 32);
    exportButton.setBounds(100, 234, 64, 32);
    demo.setBounds(28, 282, 64, 32);
    clear.setBounds(100, 282, 64, 32);
    undoButton.setBounds(28, 324, 64, 32);
    redoButton.setBounds(100, 324, 64, 32);
    loop.setBounds(28, 394, 64, 32);
    metronome.setBounds(100, 394, 64, 32);
    instrument.setBounds(28, 456, 136, 32);
    mixerButton.setBounds(28, 498, 136, 32);
    audioSettings.setBounds(28, getHeight() - 98, 136, 32);
    master.setBounds(getWidth() - 77, getHeight() / 2 + 84, 50, std::max(50, getHeight() / 2 - 188));
}
bool MainComponent::keyPressed(const juce::KeyPress &key)
{
    if (key == juce::KeyPress::F2Key)
    {
        projectName.showEditor();
        return true;
    }
    if (key == juce::KeyPress::F3Key)
    {
        showMixer(!mixer.isVisible());
        return true;
    }
    if (key.getModifiers().isCommandDown())
    {
        if (key.getKeyCode() == 'T')
        {
            studio.createTrack(Track::Instrument::sine);
            return true;
        }
        if (key.getKeyCode() == 'S')
        {
            save(key.getModifiers().isShiftDown());
            return true;
        }
        if (key.getKeyCode() == 'O')
        {
            open();
            return true;
        }
        if (key.getKeyCode() == 'E')
        {
            exportAudio();
            return true;
        }
        if (key.getKeyCode() == 'N')
        {
            newButton.onClick();
            return true;
        }
    }
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Z')
    {
        key.getModifiers().isShiftDown() ? redo() : undo();
        return true;
    }
    if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Y')
    {
        redo();
        return true;
    }
    if (key == juce::KeyPress::spaceKey)
    {
        play.onClick();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        stop.onClick();
        return true;
    }
    return false;
}
bool MainComponent::runSmokeChecks()
{
    const auto originalName = project.name;
    projectName.setText("Midnight sketch", juce::sendNotificationSync);
    if (project.name != "Midnight sketch")
        return false;
    undo();
    if (project.name != originalName)
        return false;
    projectName.setText("", juce::sendNotificationSync);
    if (project.name != originalName || projectName.getText() != juce::String(originalName))
        return false;
    if (!exerciseProjectStore(juce::File::getCurrentWorkingDirectory().getChildFile("persistence-smoke")))
        return false;
    if (!exerciseSessionStore(juce::File::getCurrentWorkingDirectory().getChildFile("session-smoke")))
        return false;
    std::array<float, 512> left{}, right{};
    float *outputs[]{left.data(), right.data()};
    const auto pump = [this, &outputs]
    {
        for (int i = 0; i < 3; ++i)
        {
            engine.process(outputs, 2, 512);
            timerCallback();
        }
    };
    play.onClick();
    pump();
    if (!engine.meters.playing.load() || engine.meters.positionSamples.load() <= 0)
        return false;
    stop.onClick();
    pump();
    if (engine.meters.playing.load() || engine.meters.positionSamples.load() != 0)
        return false;
    tempo.setValue(140, juce::sendNotificationSync);
    pump();
    if (project.bpm != 140)
        return false;
    tempo.setValue(124, juce::sendNotificationSync);
    pump();
    for (int theme : {1, 2, 3})
        themes.setSelectedId(theme, juce::sendNotificationSync);
    if (look.tokens.accent != ThemeTokens::ember().accent || !studio.exerciseEditing() ||
        !mixer.exerciseEditing())
        return false;
    pump();
    undo();
    redo();
    pump();
    project = makeDemoProject();
    studio.resetSelection();
    undoHistory.clear();
    redoHistory.clear();
    dirty_ = false;
    publishProject(false);
    pump();
    for (auto size : {juce::Point<int>(960, 640), juce::Point<int>(1280, 800)})
    {
        setSize(size.x, size.y);
        for (int i = 0; i < getNumChildComponents(); ++i)
            if (!getLocalBounds().contains(getChildComponent(i)->getBounds()))
                return false;
    }
    return validateProject(project).empty();
}
bool MainComponent::runDeviceChecks() const
{
    return device.available() && engine.meters.callbackCount.load() > 0 && !engine.meters.playing.load() &&
           engine.meters.masterPeak.load() < 0.0001f;
}
} // namespace opendaw
