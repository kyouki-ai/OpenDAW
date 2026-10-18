#pragma once
#include "persistence/ProjectStore.h"

namespace opendaw
{
struct Preferences
{
    int theme = 3, width = 1280, height = 800;
    juce::String deviceXml;
};
class SessionStore
{
  public:
    explicit SessionStore(juce::File folder) : folder_(std::move(folder)) {}
    [[nodiscard]] Preferences preferences() const;
    [[nodiscard]] juce::Result savePreferences(const Preferences &) const;
    [[nodiscard]] juce::File recoveryFile() const
    {
        return folder_.getChildFile("recovery.opendaw");
    }
    [[nodiscard]] juce::Result checkpoint(const Project &) const;
    [[nodiscard]] bool hasRecovery() const;
    [[nodiscard]] juce::Result loadRecovery(Project &) const;
    [[nodiscard]] bool clearRecovery() const;

  private:
    juce::File folder_;
};
[[nodiscard]] bool exerciseSessionStore(const juce::File &);
} // namespace opendaw
