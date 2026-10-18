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
    void initialise(const juce::String &) override
    {
        window = std::make_unique<Window>();
        const auto args = getCommandLineParameterArray();
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
        quit();
    }

  private:
    class Window final : public juce::DocumentWindow
    {
      public:
        Window() : DocumentWindow("OpenDAW - Studio Preview", juce::Colour(0xff101419), allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new opendaw::MainComponent(), true);
            setResizable(true, true);
            setResizeLimits(960, 640, 3840, 2160);
            centreWithSize(1280, 800);
            setVisible(true);
        }
        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(OpenDAWApplication)
