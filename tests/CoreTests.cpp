#include "audio/Transport.h"
#include "core/Editing.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
int failures = 0;
void check(bool condition, const char *name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    if (!condition)
        ++failures;
}
template <typename Exception, typename Fn> void throws(Fn fn, const char *name)
{
    try
    {
        fn();
        check(false, name);
    }
    catch (const Exception &)
    {
        check(true, name);
    }
    catch (...)
    {
        check(false, name);
    }
}
} // namespace
int main()
{
    using namespace opendaw;
    check(ticksToSamples(ppq, 120, 48000) == 24000, "quarter note at 120 BPM / 48 kHz");
    check(ticksToSamples(ppq * 4, 120, 44100) == 88200, "bar at 44.1 kHz");
    check(ticksToSamples(0, 124, 48000) == 0, "origin");
    check(ticksToSamples(1, 120, 48000) == 25, "single tick");
    check(ticksToSamples(1, 124, 44100) == 22, "fractional sample rounding");
    throws<std::invalid_argument>([] { (void)ticksToSamples(-1, 120, 48000); }, "negative ticks");
    throws<std::invalid_argument>([] { (void)ticksToSamples(1, 0, 48000); }, "zero tempo");
    throws<std::invalid_argument>([] { (void)ticksToSamples(1, 120, NAN); }, "NaN sample rate");
    throws<std::overflow_error>([] { (void)ticksToSamples(INT64_MAX, 20, 192000); }, "sample overflow");
    Transport transport;
    check(!transport.isPlaying() && transport.position() == 0, "transport initial state");
    check(transport.setTempo(20) && transport.setTempo(300), "tempo inclusive boundaries");
    check(!transport.setTempo(NAN) && !transport.setTempo(301) && transport.tempo() == 300,
          "invalid tempo preserves previous value");
    transport.play();
    transport.play();
    check(transport.isPlaying(), "play is idempotent");
    transport.stop();
    check(!transport.isPlaying() && transport.position() == 0, "stop resets transport");
    const EntityId id{"9646eac2-42a2-4a63-8f59-e071284197a8"};
    auto project = Project{id};
    const auto copied = project;
    check(copied.id == id && copied.schemaVersion == 1, "project copy preserves identity");
    auto session = makeDemoProject();
    const auto added = addInstrumentTrack(session, Track::Instrument::sine);
    check(added.error.empty() && session.tracks.size() == 6 && session.patterns.size() == 6 &&
              session.tracks.back().patternId == session.patterns.back().id &&
              validateProject(session).empty(),
          "add instrument creates a linked empty pattern and arrangement clip");
    const auto stableTrack = session.tracks[0].id;
    check(renameTrack(session, session.tracks[0].id, "Deep kick").error.empty() &&
              session.tracks[0].name == "Deep kick" && session.tracks[0].id == stableTrack,
          "rename preserves identity even when passed a reference into the model");
    check(!renameTrack(session, stableTrack, "").error.empty() && session.tracks[0].name == "Deep kick",
          "invalid rename preserves model");
    session.tracks[4].filterEnabled = true;
    session.tracks[4].filterCutoff = 1200;
    const auto sourcePattern = session.tracks[4].patternId;
    const auto duplicated = duplicateTrack(session, session.tracks[4].id);
    const auto clonedPattern = session.tracks.back().patternId;
    const auto originalPattern =
        std::find_if(session.patterns.begin(), session.patterns.end(),
                     [&sourcePattern](const Pattern &p) { return p.id == sourcePattern; });
    const auto clone = std::find_if(session.patterns.begin(), session.patterns.end(),
                                    [&clonedPattern](const Pattern &p) { return p.id == clonedPattern; });
    check(duplicated.error.empty() && sourcePattern != clonedPattern && clone != session.patterns.end() &&
              clone->notes.size() == originalPattern->notes.size() &&
              clone->notes[0].id != originalPattern->notes[0].id &&
              clone->notes[0].pitch == originalPattern->notes[0].pitch &&
              session.tracks.back().filterEnabled && session.tracks.back().filterCutoff == 1200 &&
              validateProject(session).empty(),
          "duplicate creates independent notes and clips while preserving channel effects");
    const auto remaining = session.tracks.back().id;
    check(removeTrack(session, stableTrack).error.empty() && session.tracks.size() == 6 &&
              std::any_of(session.tracks.begin(), session.tracks.end(),
                          [&remaining](const Track &t) { return t.id == remaining; }) &&
              validateProject(session).empty(),
          "delete track preserves other tracks and pattern references");
    session.tracks.back().clips.clear();
    const auto emptyTrackPattern = session.tracks.back().patternId;
    check(duplicateTrack(session, session.tracks.back().id).error.empty() &&
              session.tracks.back().clips.empty() && session.tracks.back().patternId != emptyTrackPattern &&
              !session.tracks.back().patternId.value.empty(),
          "a clip-free track retains an independently duplicated editor pattern");
    while (session.tracks.size() < 8)
        (void)addInstrumentTrack(session, Track::Instrument::hat);
    const auto patternsBefore = session.patterns.size();
    check(!addInstrumentTrack(session, Track::Instrument::kick).error.empty() &&
              !duplicateTrack(session, session.tracks[0].id).error.empty() &&
              session.patterns.size() == patternsBefore,
          "track limit rejects edits without partial new patterns");
    session.denominator = 0;
    check(!addInstrumentTrack(session, Track::Instrument::sine).error.empty(),
          "invalid clock rejected before calculating a new track length");
    Pattern phrase{newId(), "Loose timing", ppq * 4};
    for (const auto start : {119, 120, 121, 3830, 137})
        phrase.notes.push_back({newId(), start, start == 3830 ? 10 : 240, 64, 0.6f});
    phrase.notes.push_back({newId(), 0, phrase.length, 60, 0.4f});
    const auto loose = phrase;
    std::vector<std::string> selection;
    for (std::size_t i : {0u, 1u, 2u, 3u, 5u})
        selection.push_back(phrase.notes[i].id.value);
    check(quantizeNoteStarts(phrase, selection, ppq / 4) && phrase.notes[0].start == 0 &&
              phrase.notes[1].start == 240 && phrase.notes[2].start == 240 && phrase.notes[3].start == 3600 &&
              phrase.notes[4].start == 137 && phrase.notes[5].start == 0,
          "quantize rounds nearest, ties forward, preserves unselected notes and fits pattern end");
    bool expressiveDataIntact = phrase.id == loose.id && phrase.length == loose.length;
    for (std::size_t i = 0; i < phrase.notes.size(); ++i)
        expressiveDataIntact = expressiveDataIntact && phrase.notes[i].id == loose.notes[i].id &&
                               phrase.notes[i].duration == loose.notes[i].duration &&
                               phrase.notes[i].pitch == loose.notes[i].pitch &&
                               phrase.notes[i].velocity == loose.notes[i].velocity;
    check(expressiveDataIntact && !quantizeNoteStarts(phrase, selection, ppq / 4),
          "quantize preserves identity and expression, and repeating it is a no-op");
    for (const Tick grid : {ppq / 8, ppq / 4, ppq / 2})
    {
        auto gridPhrase = loose;
        gridPhrase.notes[0].start = 301;
        check(quantizeNoteStarts(gridPhrase, {gridPhrase.notes[0].id.value}, grid) &&
                  gridPhrase.notes[0].start == (grid == 120   ? 360
                                                : grid == 240 ? 240
                                                              : 480),
              "quantize respects 1/32, 1/16 and 1/8 grid");
    }
    auto rejected = loose;
    check(!quantizeNoteStarts(rejected, selection, 0) && !quantizeNoteStarts(rejected, selection, -1) &&
              !quantizeNoteStarts(rejected, selection, INT64_MAX) && !quantizeNoteStarts(rejected, {}, 240) &&
              !quantizeNoteStarts(rejected, {"unknown-note"}, 240) && rejected.notes[0].start == 119,
          "invalid grid, empty and stale selections leave timing intact");
    rejected.notes.back().duration = INT64_MAX;
    check(!quantizeNoteStarts(rejected, selection, 240) && rejected.notes[0].start == 119,
          "invalid later note rejects quantize before changing an earlier note");
    return failures == 0 ? 0 : 1;
}
