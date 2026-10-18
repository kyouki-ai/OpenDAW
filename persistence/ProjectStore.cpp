#include "persistence/ProjectStore.h"
#include <cmath>
#include <stdexcept>
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
namespace
{
juce::var object()
{
    return juce::var(new juce::DynamicObject());
}
void set(juce::var &object, const char *name, juce::var value)
{
    object.getDynamicObject()->setProperty(name, value);
}
void set(juce::var &object, const char *name, const std::string &value)
{
    set(object, name, juce::var(juce::String(value)));
}
void set(juce::var &object, const char *name, const char *value)
{
    set(object, name, juce::var(value));
}
juce::var integer(Tick value)
{
    return juce::var(static_cast<juce::int64>(value));
}
const juce::var &field(const juce::var &value, const char *name)
{
    if (!value.isObject() || !value.getDynamicObject()->hasProperty(name))
        throw std::runtime_error("Missing project field");
    return value.getDynamicObject()->getProperty(name);
}
std::string string(const juce::var &value)
{
    if (!value.isString())
        throw std::runtime_error("Expected a string");
    return value.toString().toStdString();
}
Tick number(const juce::var &value)
{
    if (!value.isInt() && !value.isInt64())
        throw std::runtime_error("Expected an integer");
    const auto result = static_cast<juce::int64>(value);
    if (result < -100000000 || result > 100000000)
        throw std::runtime_error("Integer exceeds project bounds");
    return result;
}
double real(const juce::var &value)
{
    if (!value.isInt() && !value.isInt64() && !value.isDouble())
        throw std::runtime_error("Expected a number");
    const auto result = static_cast<double>(value);
    if (!std::isfinite(result))
        throw std::runtime_error("Non-finite project number");
    return result;
}
bool boolean(const juce::var &value)
{
    if (!value.isBool())
        throw std::runtime_error("Expected a boolean");
    return static_cast<bool>(value);
}
const juce::Array<juce::var> &array(const juce::var &value, int limit)
{
    const auto *values = value.getArray();
    if (!values || values->size() > limit)
        throw std::runtime_error("Array exceeds project bounds");
    return *values;
}
} // namespace
juce::String projectJson(const Project &p)
{
    auto root = object();
    set(root, "format", "OpenDAW");
    set(root, "version", p.schemaVersion);
    set(root, "id", p.id.value);
    set(root, "name", p.name);
    set(root, "bpm", p.bpm);
    set(root, "numerator", p.numerator);
    set(root, "denominator", p.denominator);
    set(root, "loopStart", integer(p.loopStart));
    set(root, "loopEnd", integer(p.loopEnd));
    set(root, "loop", p.loopEnabled);
    set(root, "metronome", p.metronome);
    set(root, "masterGain", p.masterGain);
    juce::Array<juce::var> patterns, tracks;
    for (const auto &pattern : p.patterns)
    {
        auto item = object();
        set(item, "id", pattern.id.value);
        set(item, "name", pattern.name);
        set(item, "length", integer(pattern.length));
        juce::Array<juce::var> notes;
        for (const auto &note : pattern.notes)
        {
            auto n = object();
            set(n, "id", note.id.value);
            set(n, "start", integer(note.start));
            set(n, "duration", integer(note.duration));
            set(n, "pitch", note.pitch);
            set(n, "velocity", note.velocity);
            notes.add(n);
        }
        set(item, "notes", notes);
        patterns.add(item);
    }
    for (const auto &track : p.tracks)
    {
        auto item = object();
        set(item, "id", track.id.value);
        set(item, "name", track.name);
        set(item, "instrument", static_cast<int>(track.instrument));
        set(item, "gain", track.gain);
        set(item, "pan", track.pan);
        set(item, "mute", track.mute);
        set(item, "solo", track.solo);
        juce::Array<juce::var> clips;
        for (const auto &clip : track.clips)
        {
            auto c = object();
            set(c, "id", clip.id.value);
            set(c, "pattern", clip.patternId.value);
            set(c, "start", integer(clip.start));
            set(c, "length", integer(clip.length));
            clips.add(c);
        }
        set(item, "clips", clips);
        tracks.add(item);
    }
    set(root, "patterns", patterns);
    set(root, "tracks", tracks);
    return juce::JSON::toString(root, false, 9);
}
juce::Result parseProjectJson(const juce::String &text, Project &output)
{
    if (text.getNumBytesAsUTF8() > 2 * 1024 * 1024)
        return juce::Result::fail("Project exceeds 2 MiB");
    int depth = 0;
    bool quoted = false, escaped = false;
    for (const auto character : text)
    {
        if (escaped)
        {
            escaped = false;
            continue;
        }
        if (quoted && character == '\\')
        {
            escaped = true;
            continue;
        }
        if (character == '"')
        {
            quoted = !quoted;
            continue;
        }
        if (!quoted && (character == '{' || character == '[') && ++depth > 32)
            return juce::Result::fail("Project nesting is too deep");
        if (!quoted && (character == '}' || character == ']'))
            --depth;
    }
    juce::var root;
    if (const auto result = juce::JSON::parse(text, root); result.failed())
        return result;
    try
    {
        if (string(field(root, "format")) != "OpenDAW" || number(field(root, "version")) != 1)
            return juce::Result::fail("Unsupported OpenDAW project version");
        Project p;
        p.id = {string(field(root, "id"))};
        p.name = string(field(root, "name"));
        p.bpm = real(field(root, "bpm"));
        p.numerator = static_cast<int>(number(field(root, "numerator")));
        p.denominator = static_cast<int>(number(field(root, "denominator")));
        p.loopStart = number(field(root, "loopStart"));
        p.loopEnd = number(field(root, "loopEnd"));
        p.loopEnabled = boolean(field(root, "loop"));
        p.metronome = boolean(field(root, "metronome"));
        p.masterGain = static_cast<float>(real(field(root, "masterGain")));
        for (const auto &item : array(field(root, "patterns"), 64))
        {
            Pattern pattern;
            pattern.id = {string(field(item, "id"))};
            pattern.name = string(field(item, "name"));
            pattern.length = number(field(item, "length"));
            for (const auto &n : array(field(item, "notes"), 512))
                pattern.notes.push_back({{string(field(n, "id"))},
                                         number(field(n, "start")),
                                         number(field(n, "duration")),
                                         static_cast<int>(number(field(n, "pitch"))),
                                         static_cast<float>(real(field(n, "velocity")))});
            p.patterns.push_back(std::move(pattern));
        }
        for (const auto &item : array(field(root, "tracks"), 8))
        {
            Track track;
            track.id = {string(field(item, "id"))};
            track.name = string(field(item, "name"));
            track.instrument = static_cast<Track::Instrument>(number(field(item, "instrument")));
            track.gain = static_cast<float>(real(field(item, "gain")));
            track.pan = static_cast<float>(real(field(item, "pan")));
            track.mute = boolean(field(item, "mute"));
            track.solo = boolean(field(item, "solo"));
            for (const auto &c : array(field(item, "clips"), 256))
                track.clips.push_back({{string(field(c, "id"))},
                                       {string(field(c, "pattern"))},
                                       number(field(c, "start")),
                                       number(field(c, "length"))});
            p.tracks.push_back(std::move(track));
        }
        if (const auto error = validateProject(p); !error.empty())
            return juce::Result::fail(error);
        output = std::move(p);
        return juce::Result::ok();
    }
    catch (const std::exception &error)
    {
        return juce::Result::fail(error.what());
    }
}
juce::Result saveProject(const Project &project, const juce::File &target)
{
    if (const auto error = validateProject(project); !error.empty())
        return juce::Result::fail(error);
    if (!target.getParentDirectory().isDirectory())
        return juce::Result::fail("Project folder does not exist");
    const juce::TemporaryFile temp(target);
    {
        auto stream = temp.getFile().createOutputStream();
        if (!stream)
            return juce::Result::fail("Cannot create temporary project file");
        if (!stream->writeText(projectJson(project), false, false, "\n"))
            return juce::Result::fail("Project write failed");
        stream->flush();
        if (stream->getStatus().failed())
            return stream->getStatus();
    }
    const auto backup = target.getSiblingFile(target.getFileName() + ".bak");
    if (target.existsAsFile() && !target.copyFileTo(backup))
        return juce::Result::fail("Cannot create project backup");
#if JUCE_WINDOWS
    if (!MoveFileExW(temp.getFile().getFullPathName().toWideCharPointer(),
                     target.getFullPathName().toWideCharPointer(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return juce::Result::fail("Atomic project replacement failed: " +
                                  juce::String(static_cast<int>(GetLastError())));
#else
    std::error_code error;
    std::filesystem::rename(temp.getFile().getFullPathName().toStdString(),
                            target.getFullPathName().toStdString(), error);
    if (error)
        return juce::Result::fail(error.message());
#endif
    return juce::Result::ok();
}
juce::Result loadProject(const juce::File &file, Project &project)
{
    if (!file.existsAsFile())
        return juce::Result::fail("Project file is missing");
    if (file.getSize() > 2 * 1024 * 1024)
        return juce::Result::fail("Project exceeds 2 MiB");
    return parseProjectJson(file.loadFileAsString(), project);
}
bool exerciseProjectStore(const juce::File &folder)
{
    if (folder.createDirectory().failed())
        return false;
    auto original = makeDemoProject();
    original.name = "After hours - round trip";
    original.tracks[0].pan = -0.25f;
    original.tracks[1].solo = true;
    const auto file = folder.getChildFile("roundtrip.opendaw");
    Project loaded;
    if (saveProject(original, file).failed() || loadProject(file, loaded).failed() ||
        projectJson(original) != projectJson(loaded))
        return false;
    auto changed = original;
    changed.bpm = 137;
    if (saveProject(changed, file).failed())
        return false;
    Project backup;
    if (loadProject(file.getSiblingFile(file.getFileName() + ".bak"), backup).failed() ||
        backup.id != original.id || backup.bpm != original.bpm)
        return false;
    const auto intact = projectJson(loaded);
    if (parseProjectJson("{ broken", loaded).wasOk() || projectJson(loaded) != intact)
        return false;
    const auto future = projectJson(original).replace("\"version\": 1", "\"version\": 99");
    if (parseProjectJson(future, loaded).wasOk())
        return false;
    if (loadProject(folder.getChildFile("missing.opendaw"), loaded).wasOk() || projectJson(loaded) != intact)
        return false;
    auto invalid = original;
    invalid.patterns[0].notes[0].velocity = 2;
    if (saveProject(invalid, file).wasOk() || loadProject(file, loaded).failed() || loaded.bpm != 137)
        return false;
    return true;
}
} // namespace opendaw
