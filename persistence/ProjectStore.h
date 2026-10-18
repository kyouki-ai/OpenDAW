#pragma once
#include "core/Project.h"
#include <juce_core/juce_core.h>

namespace opendaw
{
[[nodiscard]] juce::String projectJson(const Project &);
[[nodiscard]] juce::Result parseProjectJson(const juce::String &, Project &);
[[nodiscard]] juce::Result saveProject(const Project &, const juce::File &);
[[nodiscard]] juce::Result loadProject(const juce::File &, Project &);
[[nodiscard]] bool exerciseProjectStore(const juce::File &testDirectory);
} // namespace opendaw
