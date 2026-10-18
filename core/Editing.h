#pragma once
#include "core/Project.h"

namespace opendaw
{
struct EditResult
{
    std::string error;
    EntityId trackId;
};
// Message thread only. A failed edit leaves the original model intact.
[[nodiscard]] EditResult addInstrumentTrack(Project &, Track::Instrument);
[[nodiscard]] EditResult duplicateTrack(Project &, const EntityId &);
[[nodiscard]] EditResult removeTrack(Project &, const EntityId &);
[[nodiscard]] EditResult renameTrack(Project &, const EntityId &, const std::string &name);
// Nearest legal grid position; ties move forward. Duration, pitch, velocity and IDs stay intact.
// Returns false without modification for invalid timing, empty selection or an already aligned pattern.
[[nodiscard]] bool quantizeNoteStarts(Pattern &, const std::vector<std::string> &selectedIds, Tick grid);
} // namespace opendaw
