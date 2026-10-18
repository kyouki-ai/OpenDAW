#include "core/Editing.h"
#include <algorithm>
#include <array>
#include <unordered_map>

namespace opendaw
{
namespace
{
std::string uniqueName(const Project &project, const std::string &base)
{
    auto name = base;
    for (int suffix = 2; std::any_of(project.tracks.begin(), project.tracks.end(),
                                     [&name](const Track &track) { return track.name == name; });
         ++suffix)
        name = base + " " + std::to_string(suffix);
    return name;
}
EditResult commit(Project &project, Project candidate, const EntityId &id)
{
    if (const auto error = validateProject(candidate); !error.empty())
        return {error, {}};
    EditResult result{{}, id};
    project = std::move(candidate);
    return result;
}
} // namespace
EditResult addInstrumentTrack(Project &project, Track::Instrument instrument)
{
    if (const auto error = validateProject(project); !error.empty())
        return {error, {}};
    if (project.tracks.size() >= 8 || project.patterns.size() >= 64)
        return {"This build supports eight tracks and 64 patterns.", {}};
    const int type = static_cast<int>(instrument);
    if (type < 0 || type > 4)
        return {"Unknown instrument", {}};
    auto candidate = project;
    constexpr std::array<const char *, 5> names{"Kick", "Snare", "Closed hat", "Sine synth", "Triangle bass"};
    const auto name = uniqueName(candidate, names[static_cast<std::size_t>(type)]);
    const Tick bar = ppq * project.numerator * 4 / project.denominator;
    Pattern pattern{newId(), name, std::clamp<Tick>(bar, ppq / 4, ppq * 16)};
    Track track{newId(), name};
    track.instrument = instrument;
    track.patternId = pattern.id;
    track.clips.push_back({newId(), pattern.id, 0, bar});
    const auto id = track.id;
    candidate.patterns.push_back(std::move(pattern));
    candidate.tracks.push_back(std::move(track));
    candidate.loopEnd = std::max(candidate.loopEnd, bar);
    return commit(project, std::move(candidate), id);
}
EditResult duplicateTrack(Project &project, const EntityId &id)
{
    if (const auto error = validateProject(project); !error.empty())
        return {error, {}};
    const auto source = std::find_if(project.tracks.begin(), project.tracks.end(),
                                     [&id](const Track &track) { return track.id == id; });
    if (source == project.tracks.end())
        return {"Track no longer exists", {}};
    if (project.tracks.size() >= 8)
        return {"This build supports eight tracks.", {}};
    if (source->patternId.value.empty() && source->clips.empty())
        return {"This track has no linked pattern. Use + to create a new instrument.", {}};
    auto candidate = project;
    Track copy = *source;
    copy.id = newId();
    copy.name = uniqueName(candidate, (copy.name.size() > 240 ? "Track" : copy.name) + " copy");
    std::unordered_map<std::string, EntityId> copies;
    const auto clonePattern = [&](const EntityId &patternId) -> EntityId
    {
        if (const auto existing = copies.find(patternId.value); existing != copies.end())
            return existing->second;
        const auto original =
            std::find_if(project.patterns.begin(), project.patterns.end(),
                         [&patternId](const Pattern &pattern) { return pattern.id == patternId; });
        if (original == project.patterns.end())
            return {};
        auto pattern = *original;
        pattern.id = newId();
        pattern.name = (pattern.name.size() > 240 ? "Pattern" : pattern.name) + " copy";
        for (auto &note : pattern.notes)
            note.id = newId();
        const auto newPattern = pattern.id;
        candidate.patterns.push_back(std::move(pattern));
        copies.emplace(patternId.value, newPattern);
        return newPattern;
    };
    if (!copy.patternId.value.empty())
        copy.patternId = clonePattern(copy.patternId);
    for (auto &clip : copy.clips)
    {
        clip.id = newId();
        clip.patternId = clonePattern(clip.patternId);
    }
    if (copy.patternId.value.empty() && !copy.clips.empty())
        copy.patternId = copy.clips.front().patternId;
    if (candidate.patterns.size() > 64)
        return {"Duplicating this track would exceed 64 patterns.", {}};
    const auto resultId = copy.id;
    candidate.tracks.push_back(std::move(copy));
    return commit(project, std::move(candidate), resultId);
}
EditResult removeTrack(Project &project, const EntityId &id)
{
    if (const auto error = validateProject(project); !error.empty())
        return {error, {}};
    auto candidate = project;
    if (std::erase_if(candidate.tracks, [&id](const Track &track) { return track.id == id; }) == 0)
        return {"Track no longer exists", {}};
    // Patterns remain available to other tracks and to undo.
    return commit(project, std::move(candidate), {});
}
EditResult renameTrack(Project &project, const EntityId &id, const std::string &name)
{
    if (const auto error = validateProject(project); !error.empty())
        return {error, {}};
    if (name.empty() || name.size() > 256)
        return {"Choose a shorter, non-empty track name.", {}};
    auto candidate = project;
    const auto track = std::find_if(candidate.tracks.begin(), candidate.tracks.end(),
                                    [&id](const Track &item) { return item.id == id; });
    if (track == candidate.tracks.end())
        return {"Track no longer exists", {}};
    track->name = name;
    return commit(project, std::move(candidate), id);
}
bool quantizeNoteStarts(Pattern &pattern, const std::vector<std::string> &selectedIds, Tick grid)
{
    if (pattern.length <= 0 || pattern.length > ppq * 16 || grid <= 0 || grid > pattern.length ||
        selectedIds.empty())
        return false;
    // Preflight every note so malformed input cannot leave a partly quantized pattern.
    for (const auto &note : pattern.notes)
        if (note.duration <= 0 || note.duration > pattern.length || note.start < 0 ||
            note.start > pattern.length - note.duration)
            return false;
    bool changed = false;
    for (auto &note : pattern.notes)
    {
        if (std::find(selectedIds.begin(), selectedIds.end(), note.id.value) == selectedIds.end())
            continue;
        const Tick nearest = note.start / grid + (note.start % grid >= (grid + 1) / 2 ? 1 : 0);
        const Tick last = (pattern.length - note.duration) / grid;
        const Tick start = std::min(nearest, last) * grid;
        changed = changed || start != note.start;
        note.start = start;
    }
    return changed;
}
} // namespace opendaw
