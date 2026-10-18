#include "ui/MainComponent.h"

class OpenDAWApplication final : public juce::JUCEApplication
{
  public:
    const juce::String getApplicationName() override
    {
        return "OpenDAW";
    }
    const juce::String getApplicationVersion() override
    {
        return "0.1.0";
    }
    bool moreThanOneInstanceAllowed() override
    {
        const auto args = getCommandLineParameterArray();
        return args.contains("--smoke-test") || args.contains("--device-smoke") ||
               args.contains("--render-demo");
    }
    void anotherInstanceStarted(const juce::String &) override
    {
        if (window)
            window->toFront(true);
    }
    void initialise(const juce::String &) override
    {
        const auto args = getCommandLineParameterArray();
        const int demo = args.indexOf("--render-demo");
        if (demo >= 0)
        {
            opendaw::WavResult result;
            result.error = "Missing WAV destination";
            if (demo + 1 < args.size())
            {
#if JUCE_WINDOWS
                const std::filesystem::path path(std::wstring(args[demo + 1].toWideCharPointer()));
#else
                const auto path = std::filesystem::u8path(args[demo + 1].toStdString());
#endif
                try
                {
                    result = opendaw::exportWav(opendaw::makeDemoProject(), path);
                }
                catch (const std::exception &error)
                {
                    result.error = error.what();
                }
            }
            const int report = args.indexOf("--report");
            if (report >= 0 && report + 1 < args.size())
            {
                const juce::File file(args[report + 1]);
                if (!file.replaceWithText(result.error.empty()
                                              ? "PASS offline demo: " + juce::String(result.frames) +
                                                    " frames; peak " + juce::String(result.peak, 6)
                                              : "FAIL " + juce::String(result.error)))
                    result.error = "Cannot write export report";
            }
            setApplicationReturnValue(result.error.empty() ? 0 : 2);
            quit();
            return;
        }
        window = std::make_unique<Window>(args.contains("--smoke-test"), !args.contains("--device-smoke"));
        if (args.contains("--device-smoke"))
        {
            juce::Timer::callAfterDelay(
                1800,
                [this, args]
                {
                    auto *content = dynamic_cast<opendaw::MainComponent *>(window->getContentComponent());
                    const bool passed = content && content->runDeviceChecks();
                    const int report = args.indexOf("--report");
                    if (report >= 0 && report + 1 < args.size())
                    {
                        juce::File file(args[report + 1]);
                        file.getParentDirectory().createDirectory();
                        file.replaceWithText(
                            passed ? "PASS: real output device opened; callbacks received; output silent.\n"
                                   : "FAIL: output device unavailable or callbacks not confirmed.\n");
                    }
                    setApplicationReturnValue(passed ? 0 : 2);
                    quit();
                });
        }
        if (args.contains("--smoke-test"))
        {
            juce::Timer::callAfterDelay(
                400,
                [this, args]
                {
                    auto *content = dynamic_cast<opendaw::MainComponent *>(window->getContentComponent());
                    bool passed = content != nullptr && content->runSmokeChecks();
                    const int index = args.indexOf("--screenshot");
                    if (index >= 0 && index + 1 >= args.size())
                        passed = false;
                    if (passed && index >= 0 && index + 1 < args.size())
                    {
                        const juce::File file(args[index + 1]);
                        const auto save = [content](const juce::File &target)
                        {
                            if (target.getParentDirectory().createDirectory().failed())
                                return false;
                            auto stream = target.createOutputStream();
                            if (!stream)
                                return false;
                            stream->setPosition(0);
                            stream->truncate();
                            const auto image = content->createComponentSnapshot(content->getLocalBounds());
                            const bool written = juce::PNGImageFormat().writeImageToStream(image, *stream);
                            stream->flush();
                            return written && stream->getStatus().wasOk();
                        };
                        passed = save(file);
                        content->showPianoRoll(true);
                        passed = save(file.getSiblingFile("opendaw-piano.png")) && passed;
                        content->showMixer(true);
                        passed = save(file.getSiblingFile("opendaw-mixer.png")) && passed;
                        content->showMixer(false);
                        content->showPianoRoll(false);
                        content->setSize(960, 640);
                        passed = save(file.getSiblingFile("opendaw-compact.png")) && passed;
                    }
                    setApplicationReturnValue(passed ? 0 : 1);
                    quit();
                });
        }
    }
    void shutdown() override
    {
        window.reset();
    }
    void systemRequestedQuit() override
    {
        if (window)
            if (auto *content = dynamic_cast<opendaw::MainComponent *>(window->getContentComponent()))
            {
                content->requestClose([this] { quit(); });
                return;
            }
        quit();
    }

  private:
    class Window final : public juce::DocumentWindow
    {
      public:
        explicit Window(bool smokeMode, bool persistentSession)
            : DocumentWindow("OpenDAW", opendaw::ThemeTokens::ember().background, allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new opendaw::MainComponent(smokeMode, persistentSession), true);
            setResizable(true, true);
            setResizeLimits(960, 640, 3840, 2160);
            centreWithSize(getContentComponent()->getWidth(), getContentComponent()->getHeight());
            // Smoke captures render the actual components without raising a window over other apps.
            setVisible(!smokeMode && persistentSession);
        }
        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(OpenDAWApplication)
