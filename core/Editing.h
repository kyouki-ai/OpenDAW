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
} // namespace opendaw
