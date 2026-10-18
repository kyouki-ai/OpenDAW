#include "persistence/SessionStore.h"
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <filesystem>
#endif

namespace opendaw
{
Preferences SessionStore::preferences() const
{
    Preferences p;
    const auto file = folder_.getChildFile("settings.json");
    if (!file.existsAsFile() || file.getSize() > 65536)
        return p;
    const auto value = juce::JSON::parse(file.loadFileAsString());
    const auto *object = value.getDynamicObject();
    if (!object)
        return p;
    const auto &theme = object->getProperty("theme");
    const auto &width = object->getProperty("width");
    const auto &height = object->getProperty("height");
    if (theme.isInt() && static_cast<int>(theme) >= 1 && static_cast<int>(theme) <= 3)
        p.theme = static_cast<int>(theme);
    if (width.isInt() && static_cast<int>(width) >= 960 && static_cast<int>(width) <= 3840)
        p.width = static_cast<int>(width);
    if (height.isInt() && static_cast<int>(height) >= 640 && static_cast<int>(height) <= 2160)
        p.height = static_cast<int>(height);
    const auto &xml = object->getProperty("deviceXml");
    if (xml.isString() && xml.toString().length() <= 32768)
        p.deviceXml = xml.toString();
    return p;
}
juce::Result SessionStore::savePreferences(const Preferences &p) const
{
    if (p.theme < 1 || p.theme > 3 || p.width < 960 || p.width > 3840 || p.height < 640 || p.height > 2160 ||
        p.deviceXml.length() > 32768)
        return juce::Result::fail("Invalid application settings");
    if (const auto result = folder_.createDirectory(); result.failed())
        return result;
    auto *object = new juce::DynamicObject();
    const juce::var value(object);
    object->setProperty("theme", p.theme);
    object->setProperty("width", p.width);
    object->setProperty("height", p.height);
    object->setProperty("deviceXml", p.deviceXml);
    const auto target = folder_.getChildFile("settings.json");
    const juce::TemporaryFile temporary(target);
    {
        auto stream = temporary.getFile().createOutputStream();
        if (!stream || !stream->writeText(juce::JSON::toString(value), false, false, "\n"))
            return juce::Result::fail("Cannot write application settings");
        stream->flush();
        if (stream->getStatus().failed())
            return stream->getStatus();
    }
#if JUCE_WINDOWS
    if (!MoveFileExW(temporary.getFile().getFullPathName().toWideCharPointer(),
                     target.getFullPathName().toWideCharPointer(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return juce::Result::fail("Cannot replace application settings");
#else
    std::error_code error;
    std::filesystem::rename(temporary.getFile().getFullPathName().toStdString(),
                            target.getFullPathName().toStdString(), error);
    if (error)
        return juce::Result::fail(error.message());
#endif
    return juce::Result::ok();
}
juce::Result SessionStore::checkpoint(const Project &project) const
{
    if (const auto result = folder_.createDirectory(); result.failed())
        return result;
    return saveProject(project, recoveryFile());
}
bool SessionStore::clearRecovery() const
{
    const auto file = recoveryFile();
    const auto backup = file.getSiblingFile(file.getFileName() + ".bak");
    const bool cleared = !file.exists() || file.deleteFile();
    const bool backupCleared = !backup.exists() || backup.deleteFile();
    return cleared && backupCleared;
}
bool SessionStore::hasRecovery() const
{
    return recoveryFile().existsAsFile() ||
           recoveryFile().getSiblingFile("recovery.opendaw.bak").existsAsFile();
}
juce::Result SessionStore::loadRecovery(Project &project) const
{
    auto result = loadProject(recoveryFile(), project);
    if (result.failed())
        result = loadProject(recoveryFile().getSiblingFile("recovery.opendaw.bak"), project);
    return result;
}
bool exerciseSessionStore(const juce::File &folder)
{
    SessionStore store(folder);
    Preferences p;
    p.theme = 2;
    p.width = 1440;
    p.height = 900;
    p.deviceXml = "<DEVICESETUP/>";
    if (store.savePreferences(p).failed())
        return false;
    const auto loaded = store.preferences();
    if (loaded.theme != p.theme || loaded.width != p.width || loaded.height != p.height ||
        loaded.deviceXml != p.deviceXml)
        return false;
    p.theme = 90;
    if (store.savePreferences(p).wasOk() || store.preferences().theme != 2)
        return false;
    auto project = makeDemoProject();
    if (store.checkpoint(project).failed())
        return false;
    Project recovered;
    if (loadProject(store.recoveryFile(), recovered).failed() ||
        projectJson(project) != projectJson(recovered))
        return false;
    project.bpm = 135;
    if (store.checkpoint(project).failed() || !store.hasRecovery() ||
        !store.recoveryFile().replaceWithText("{ damaged"))
        return false;
    if (store.loadRecovery(recovered).failed() || recovered.bpm != 124)
        return false;
    if (!store.clearRecovery() || store.hasRecovery())
        return false;
    if (!folder.getChildFile("settings.json").replaceWithText("{ corrupt"))
        return false;
    return store.preferences().theme == 3;
}
} // namespace opendaw
