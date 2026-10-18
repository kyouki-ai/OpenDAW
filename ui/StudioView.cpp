#include "ui/StudioView.h"
#include <algorithm>
#include <cmath>

namespace opendaw
{
namespace
{
void label(juce::Graphics &g, const juce::String &s, juce::Rectangle<int> r, float size, juce::Colour colour,
           bool bold = false)
{
    g.setColour(colour);
    g.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
    g.drawText(s, r, juce::Justification::centredLeft);
}
juce::Colour trackColour(int track, const ThemeTokens &theme)
{
    if (track % 4 == 1)
        return theme.secondary;
    if (track % 4 == 2)
        return theme.accent.interpolatedWith(juce::Colours::white, 0.28f);
    if (track % 4 == 3)
        return juce::Colour(0xffd4937e);
    return theme.accent;
}
bool selected(const std::vector<std::string> &selection, const Note &note)
{
    return std::find(selection.begin(), selection.end(), note.id.value) != selection.end();
}
} // namespace
StudioView::StudioView(Project &project, AudioEngine &engine, const ThemeTokens &theme)
    : project_(project), engine_(engine), theme_(theme)
{
    setWantsKeyboardFocus(true);
    setTitle("Arrangement and pattern editor");
    topPitch_ = 66;
}
juce::Rectangle<int> StudioView::arrangement() const
{
    return {0, 0, getWidth(), std::min(300, getHeight() / 2)};
}
juce::Rectangle<int> StudioView::editor() const
{
    const int top = arrangement().getBottom() + 14;
    return {0, top, getWidth(), getHeight() - top};
}
juce::Rectangle<int> StudioView::pianoGrid() const
{
    const auto e = editor();
    return {56, e.getY() + 62, getWidth() - 72, std::max(72, e.getHeight() - 120)};
}
juce::Rectangle<int> StudioView::stepCell(int channel, int step) const
{
    auto *pattern = const_cast<StudioView *>(this)->selectedPattern();
    const int steps = pattern ? static_cast<int>(pattern->length / (ppq / 4)) : 16;
    const int width = getWidth() - 168;
    const int row = juce::jlimit(
        24, 50, (editor().getHeight() - 110) / std::max(1, static_cast<int>(project_.tracks.size())));
    return {156 + step * width / steps + 2, editor().getY() + 64 + channel * row, width / steps - 4,
            std::min(28, row - 8)};
}
Pattern *StudioView::selectedPattern()
{
    if (project_.patterns.empty() || project_.tracks.empty())
        return nullptr;
    selectedTrack_ = juce::jlimit(0, static_cast<int>(project_.tracks.size()) - 1, selectedTrack_);
    return patternForTrack(selectedTrack_);
}
Pattern *StudioView::patternForTrack(int index)
{
    if (index < 0 || index >= static_cast<int>(project_.tracks.size()) || project_.patterns.empty())
        return nullptr;
    auto &track = project_.tracks[static_cast<std::size_t>(index)];
    if (!track.clips.empty())
        for (auto &pattern : project_.patterns)
            if (pattern.id == track.clips[0].patternId)
                return &pattern;
    return &project_.patterns[static_cast<std::size_t>(index) % project_.patterns.size()];
}
void StudioView::resetSelection()
{
    selectedTrack_ = 0;
    selectedNotes_.clear();
    dragNote_ = dragClip_ = clipTrack_ = -1;
    topPitch_ = 66;
}
Tick StudioView::tickAt(float x) const
{
    auto *pattern = const_cast<StudioView *>(this)->selectedPattern();
    const auto grid = pianoGrid();
    if (!pattern)
        return 0;
    const auto raw =
        static_cast<double>(x - grid.getX()) / grid.getWidth() * static_cast<double>(pattern->length);
    return juce::jlimit<Tick>(0, pattern->length - ppq / 4,
                              static_cast<Tick>(std::floor(raw / (ppq / 4))) * (ppq / 4));
}
int StudioView::pitchAt(float y) const
{
    const auto grid = pianoGrid();
    const int rows = juce::jlimit(12, 24, grid.getHeight() / 12);
    return juce::jlimit(0, 127, topPitch_ - static_cast<int>((y - grid.getY()) / grid.getHeight() * rows));
}
juce::Rectangle<float> StudioView::noteBounds(const Note &note) const
{
    const auto grid = pianoGrid().toFloat();
    auto *pattern = const_cast<StudioView *>(this)->selectedPattern();
    if (!pattern)
        return {};
    const int rows = juce::jlimit(12, 24, static_cast<int>(grid.getHeight()) / 12);
    return {
        grid.getX() + static_cast<float>(note.start) / static_cast<float>(pattern->length) * grid.getWidth(),
        grid.getY() + static_cast<float>(topPitch_ - note.pitch) * grid.getHeight() / rows + 1,
        std::max(3.0f,
                 static_cast<float>(note.duration) / static_cast<float>(pattern->length) * grid.getWidth() -
                     2),
        grid.getHeight() / rows - 2};
}
int StudioView::noteAt(juce::Point<float> point) const
{
    auto *pattern = const_cast<StudioView *>(this)->selectedPattern();
    if (pattern)
        for (int i = static_cast<int>(pattern->notes.size()) - 1; i >= 0; --i)
            if (noteBounds(pattern->notes[static_cast<std::size_t>(i)]).contains(point))
                return i;
    return -1;
}
void StudioView::beginEdit()
{
    if (!editStarted_ && beforeEdit)
        beforeEdit();
    editStarted_ = true;
}
void StudioView::changed()
{
    if (onEdit)
        onEdit();
    repaint();
}
void StudioView::paint(juce::Graphics &g)
{
    const auto &t = theme_;
    const auto a = arrangement(), e = editor();
    for (auto r : {a, e})
    {
        g.setColour(t.surface);
        g.fillRoundedRectangle(r.toFloat(), 8);
        g.setColour(t.border.withAlpha(0.7f));
        g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 8, 1);
    }
    label(g, "Arrangement", {16, 5, 200, 26}, 16, t.text, true);
    const Tick timeline = std::max(ppq * 32, project_.loopEnd);
    const float timeWidth = static_cast<float>(getWidth() - 158);
    const auto xAt = [timeWidth, timeline](Tick tick)
    { return 150.0f + static_cast<float>(tick) / static_cast<float>(timeline) * timeWidth; };
    const int rowHeight =
        project_.tracks.empty() ? 50 : (a.getHeight() - 40) / static_cast<int>(project_.tracks.size());
    const auto playTick = engine_.meters.positionTicks.load(std::memory_order_relaxed);
    g.setColour(t.accent.withAlpha(0.07f));
    g.fillRect(xAt(project_.loopStart), 33.0f, xAt(project_.loopEnd) - xAt(project_.loopStart),
               static_cast<float>(a.getHeight() - 36));
    for (int bar = 0; bar <= 8; ++bar)
    {
        const float x = 150.0f + bar * timeWidth / 8.0f;
        g.setColour(t.border.withAlpha(0.5f));
        g.drawVerticalLine(static_cast<int>(x), 32, static_cast<float>(a.getBottom() - 4));
        if (bar < 8)
            label(g, juce::String(bar * static_cast<int>(timeline / (ppq * 32)) + 1),
                  {static_cast<int>(x) + 6, 7, 28, 18}, 10, t.muted);
    }
    for (std::size_t c = 0; c < project_.tracks.size(); ++c)
    {
        const auto &track = project_.tracks[c];
        const int y = 36 + static_cast<int>(c) * rowHeight;
        const auto colour = trackColour(static_cast<int>(c), t);
        if (static_cast<int>(c) == selectedTrack_)
        {
            g.setColour(colour.withAlpha(0.07f));
            g.fillRoundedRectangle(3.0f, static_cast<float>(y), 140.0f, static_cast<float>(rowHeight - 2), 4);
            g.setColour(colour);
            g.fillRoundedRectangle(3.0f, static_cast<float>(y + 8), 2, static_cast<float>(rowHeight - 18), 1);
        }
        label(g, juce::String(track.name), {16, y + 3, 128, 22}, 13, t.text, true);
        for (int button = 0; button < 2; ++button)
        {
            const bool on = button == 0 ? track.mute : track.solo;
            g.setColour(on ? colour : t.raised);
            g.fillRoundedRectangle(18.0f + button * 27, static_cast<float>(y + 28), 23, 20, 3);
            label(g, button == 0 ? "M" : "S", {24 + button * 27, y + 28, 16, 20}, 11,
                  on ? t.background : t.muted);
        }
        const float meter = engine_.meters.channelPeak[c].load(std::memory_order_relaxed);
        g.setColour(t.border);
        g.fillRoundedRectangle(80, static_cast<float>(y + 34), 53, 4, 2);
        g.setColour(colour);
        g.fillRoundedRectangle(80, static_cast<float>(y + 34), juce::jlimit(0.0f, 53.0f, meter * 130), 4, 2);
        g.setColour(t.muted);
        g.fillEllipse(80 + track.gain * 50, static_cast<float>(y + 31), 8, 8);
        for (const auto &clip : track.clips)
        {
            const auto box =
                juce::Rectangle<float>(xAt(clip.start) + 2, static_cast<float>(y + 2),
                                       std::max(3.0f, xAt(clip.start + clip.length) - xAt(clip.start) - 4),
                                       static_cast<float>(rowHeight - 6));
            g.saveState();
            g.reduceClipRegion(juce::Rectangle<int>(150, y, getWidth() - 155, rowHeight));
            g.setGradientFill(juce::ColourGradient(colour.withAlpha(track.mute ? 0.08f : 0.26f), box.getX(),
                                                   box.getY(), colour.withAlpha(0.08f), box.getX(),
                                                   box.getBottom(), false));
            g.fillRoundedRectangle(box, 4);
            g.setColour(colour.withAlpha(0.75f));
            g.drawRoundedRectangle(box, 4, 1);
            label(g, juce::String(track.name) + " / pattern", box.toNearestInt().reduced(8).withHeight(18),
                  10, colour);
            for (const auto &pattern : project_.patterns)
                if (pattern.id == clip.patternId)
                    for (Tick repeat = 0; repeat < clip.length; repeat += pattern.length)
                        for (const auto &note : pattern.notes)
                        {
                            if (repeat + note.start >= clip.length)
                                continue;
                            const float nx = xAt(clip.start + repeat + note.start) + 3;
                            const float ny =
                                box.getBottom() - 12 - static_cast<float>(note.pitch % 12) * 1.5f;
                            g.setColour(colour.withAlpha(0.85f));
                            g.fillRoundedRectangle(nx, ny, std::max(2.0f, xAt(note.duration) - 150 - 2), 2.5f,
                                                   1);
                        }
            g.restoreState();
        }
    }
    if (playTick <= timeline)
    {
        const float x = xAt(playTick);
        g.setColour(t.accent);
        g.drawVerticalLine(static_cast<int>(x), 30, static_cast<float>(a.getBottom() - 5));
        juce::Path marker;
        marker.addTriangle(x - 4, 27, x + 4, 27, x, 33);
        g.fillPath(marker);
    }
    for (int tab = 0; tab < 2; ++tab)
    {
        const bool on = piano_ == (tab == 1);
        label(g, tab == 0 ? "Step sequencer" : "Piano roll", {16 + tab * 132, e.getY() + 5, 130, 30}, 13,
              on ? t.accent : t.muted, on);
        if (on)
        {
            g.setColour(t.accent);
            g.fillRect(16 + tab * 132, e.getY() + 36, 115, 2);
        }
    }
    auto *pattern = selectedPattern();
    if (!pattern)
        return;
    label(g, juce::String(pattern->name), {300, e.getY() + 8, std::max(30, getWidth() - 435), 24}, 12,
          t.muted);
    label(g, juce::String(pattern->length / (ppq / 4)) + " steps", {getWidth() - 100, e.getY() + 8, 90, 24},
          12, t.accent);
    if (!piano_)
    {
        const int steps = static_cast<int>(pattern->length / (ppq / 4));
        for (std::size_t c = 0; c < project_.tracks.size(); ++c)
        {
            Pattern *p = patternForTrack(static_cast<int>(c));
            if (!p)
                continue;
            const auto colour = trackColour(static_cast<int>(c), t);
            const auto labelBounds = stepCell(static_cast<int>(c), 0);
            label(g, juce::String(project_.tracks[c].name),
                  {16, labelBounds.getY(), 132, labelBounds.getHeight()}, 12, t.text);
            for (int step = 0; step < steps; ++step)
            {
                const auto r = stepCell(static_cast<int>(c), step).toFloat();
                float velocity = 0;
                for (const auto &note : p->notes)
                    if (note.start == step * ppq / 4)
                        velocity = note.velocity;
                g.setColour(velocity > 0          ? colour.withAlpha(0.4f + velocity * 0.6f)
                            : (step / 4) % 2 == 0 ? t.raised
                                                  : t.border.withAlpha(0.5f));
                g.fillRoundedRectangle(r, 3);
                if (velocity > 0)
                {
                    g.setColour(t.background.withAlpha(0.7f));
                    g.fillRect(r.getX() + 3, r.getBottom() - 5, (r.getWidth() - 6) * velocity, 2.0f);
                }
                if (engine_.meters.playing.load() &&
                    static_cast<int>((playTick % p->length) / (ppq / 4)) == step)
                {
                    g.setColour(t.text);
                    g.drawRoundedRectangle(r.expanded(1), 3, 1.5f);
                }
            }
        }
        label(g, "Click a step to toggle / scroll for velocity / select a track for piano roll",
              {16, e.getBottom() - 28, getWidth() - 32, 22}, 11, t.muted);
        return;
    }
    const auto grid = pianoGrid();
    const int rows = juce::jlimit(12, 24, grid.getHeight() / 12);
    for (int row = 0; row < rows; ++row)
    {
        const int pitch = topPitch_ - row, note = (pitch % 12 + 12) % 12;
        const bool black = note == 1 || note == 3 || note == 6 || note == 8 || note == 10;
        const int y = grid.getY() + row * grid.getHeight() / rows;
        const int next = grid.getY() + (row + 1) * grid.getHeight() / rows;
        g.setColour(black ? t.background : t.raised.withAlpha(0.55f));
        g.fillRect(grid.getX(), y, grid.getWidth(), next - y);
        g.setColour(black ? t.background.brighter(0.1f) : t.text.darker(0.1f));
        g.fillRect(8, y, black ? 31 : 43, next - y - 1);
        if (note == 0)
            label(g, "C" + juce::String(pitch / 12 - 1), {12, y, 32, next - y}, 9,
                  black ? t.text : t.background);
    }
    for (int step = 0; step <= pattern->length / (ppq / 4); ++step)
    {
        const int x = grid.getX() + static_cast<int>(static_cast<double>(step * ppq / 4) / pattern->length *
                                                     grid.getWidth());
        g.setColour(t.border.withAlpha(step % 4 == 0 ? 0.8f : 0.3f));
        g.drawVerticalLine(x, static_cast<float>(grid.getY()), static_cast<float>(grid.getBottom()));
    }
    g.saveState();
    g.reduceClipRegion(grid);
    for (const auto &note : pattern->notes)
    {
        const auto box = noteBounds(note);
        g.setColour(selected(selectedNotes_, note) ? t.text
                                                   : t.accent.withAlpha(0.5f + note.velocity * 0.5f));
        g.fillRoundedRectangle(box, 2);
        g.setColour(t.background.withAlpha(0.3f));
        g.drawVerticalLine(static_cast<int>(box.getRight() - 4), box.getY() + 2, box.getBottom() - 2);
    }
    g.restoreState();
    const int velocityY = grid.getBottom() + 12, velocityH = std::max(10, e.getBottom() - velocityY - 22);
    label(g, "VEL", {10, velocityY, 40, 18}, 9, t.muted);
    for (const auto &note : pattern->notes)
    {
        const float x = noteBounds(note).getX() + 3;
        g.setColour(selected(selectedNotes_, note) ? t.text : t.accent);
        g.drawLine(x, static_cast<float>(velocityY + velocityH), x,
                   static_cast<float>(velocityY) + (1 - note.velocity) * velocityH, 2);
    }
    label(g,
          "Click: add / right-click: delete / drag: move / edge: length / Shift: select / Ctrl+D: duplicate",
          {12, e.getBottom() - 18, getWidth() - 24, 16}, 10, t.muted);
}
void StudioView::toggleStep(int channel, int step)
{
    auto *target = patternForTrack(channel);
    if (!target)
        return;
    auto &pattern = *target;
    const Tick tick = step * ppq / 4;
    if (tick >= pattern.length)
        return;
    beginEdit();
    const bool active = std::any_of(pattern.notes.begin(), pattern.notes.end(),
                                    [tick](const Note &n) { return n.start == tick; });
    if (active)
        std::erase_if(pattern.notes, [tick](const Note &n) { return n.start == tick; });
    else
        pattern.notes.push_back(
            {newId(), tick, std::min(ppq / 4, pattern.length - tick), channel == 3 ? 45 : 60, 0.8f});
    changed();
}
void StudioView::mouseDown(const juce::MouseEvent &event)
{
    grabKeyboardFocus();
    editStarted_ = false;
    gainDrag_ = loopDrag_ = velocityDrag_ = false;
    dragNote_ = dragClip_ = clipTrack_ = -1;
    dragOrigin_ = event.position;
    dragTimeline_ = std::max(ppq * 32, project_.loopEnd);
    const auto a = arrangement(), e = editor();
    auto *pattern = selectedPattern();
    if (e.contains(event.getPosition()) && event.y < e.getY() + 40)
    {
        if (event.x < 280)
        {
            piano_ = event.x >= 145;
            selectedNotes_.clear();
            repaint();
            return;
        }
        if (event.x > getWidth() - 110 && pattern)
        {
            beginEdit();
            pattern->length = pattern->length == ppq * 4 ? ppq * 8 : ppq * 4;
            std::erase_if(pattern->notes, [pattern](const Note &n) { return n.start >= pattern->length; });
            for (auto &n : pattern->notes)
                n.duration = std::min(n.duration, pattern->length - n.start);
            changed();
            return;
        }
    }
    if (a.contains(event.getPosition()))
    {
        const Tick timeline = dragTimeline_;
        const auto tick =
            static_cast<Tick>(static_cast<double>(event.x - 150) / (getWidth() - 158) * timeline);
        if (event.y < 34 && event.x >= 150)
        {
            if (event.mods.isShiftDown())
            {
                beginEdit();
                loopDrag_ = true;
                project_.loopStart = std::max<Tick>(0, tick / (ppq * 4) * (ppq * 4));
                project_.loopEnd = std::max(project_.loopEnd, project_.loopStart + ppq * 4);
                changed();
            }
            else
                engine_.seek(std::max<Tick>(0, tick));
            return;
        }
        const int rh = (a.getHeight() - 40) / std::max(1, static_cast<int>(project_.tracks.size()));
        const int channel = (event.y - 36) / rh;
        if (channel < 0 || channel >= static_cast<int>(project_.tracks.size()))
            return;
        selectedTrack_ = channel;
        topPitch_ = channel == 3 ? 54 : 66;
        selectedNotes_.clear();
        auto &track = project_.tracks[static_cast<std::size_t>(channel)];
        const int localY = event.y - 36 - channel * rh;
        if (event.x < 150)
        {
            if (localY >= 28 && localY < 49 && event.x >= 18 && event.x < 70)
            {
                beginEdit();
                if (event.x < 44)
                    track.mute = !track.mute;
                else
                    track.solo = !track.solo;
                changed();
            }
            if (localY >= 25 && localY < 45 && event.x >= 78)
            {
                beginEdit();
                gainDrag_ = true;
                mouseDrag(event);
            }
            repaint();
            return;
        }
        for (std::size_t i = 0; i < track.clips.size(); ++i)
        {
            auto &clip = track.clips[i];
            if (tick >= clip.start && tick < clip.start + clip.length)
            {
                if (event.mods.isRightButtonDown())
                {
                    beginEdit();
                    track.clips.erase(track.clips.begin() + static_cast<std::ptrdiff_t>(i));
                    changed();
                    return;
                }
                if (event.mods.isShiftDown())
                {
                    beginEdit();
                    auto copy = clip;
                    copy.id = newId();
                    copy.start += copy.length;
                    project_.loopEnd = std::max(project_.loopEnd, copy.start + copy.length);
                    track.clips.push_back(copy);
                    changed();
                    return;
                }
                dragClip_ = static_cast<int>(i);
                clipTrack_ = channel;
                originalStart_ = clip.start;
                originalDuration_ = clip.length;
                resizeClip_ =
                    static_cast<double>(clip.start + clip.length - tick) / timeline * (getWidth() - 158) < 9;
                return;
            }
        }
        if (pattern)
        {
            beginEdit();
            const Tick start = std::max<Tick>(0, tick / (ppq * 4) * (ppq * 4));
            track.clips.push_back({newId(), selectedPattern()->id, start, ppq * 4});
            project_.loopEnd = std::max(project_.loopEnd, start + ppq * 4);
            changed();
        }
        return;
    }
    if (!pattern)
        return;
    if (!piano_)
    {
        const int steps = static_cast<int>(pattern->length / (ppq / 4));
        for (int channel = 0; channel < static_cast<int>(project_.tracks.size()); ++channel)
            for (int step = 0; step < steps; ++step)
                if (stepCell(channel, step).contains(event.getPosition()))
                {
                    selectedTrack_ = channel;
                    toggleStep(channel, step);
                    return;
                }
        return;
    }
    const auto grid = pianoGrid();
    if (event.y > grid.getBottom() && event.x >= grid.getX())
    {
        float nearest = 12;
        for (std::size_t i = 0; i < pattern->notes.size(); ++i)
            if (const float distance = std::abs(event.position.x - noteBounds(pattern->notes[i]).getX() - 3);
                distance < nearest)
            {
                nearest = distance;
                dragNote_ = static_cast<int>(i);
            }
        if (dragNote_ >= 0)
        {
            beginEdit();
            velocityDrag_ = true;
            mouseDrag(event);
        }
        return;
    }
    if (!grid.contains(event.getPosition()))
        return;
    dragNote_ = noteAt(event.position);
    if (dragNote_ >= 0)
    {
        auto &note = pattern->notes[static_cast<std::size_t>(dragNote_)];
        if (event.mods.isRightButtonDown())
        {
            beginEdit();
            pattern->notes.erase(pattern->notes.begin() + dragNote_);
            selectedNotes_.clear();
            dragNote_ = -1;
            changed();
            return;
        }
        if (!event.mods.isShiftDown() && !selected(selectedNotes_, note))
            selectedNotes_.clear();
        if (!selected(selectedNotes_, note))
            selectedNotes_.push_back(note.id.value);
        originalStart_ = note.start;
        originalDuration_ = note.duration;
        originalPitch_ = note.pitch;
        resizeNote_ = event.position.x > noteBounds(note).getRight() - 8;
        repaint();
        return;
    }
    if (event.mods.isRightButtonDown())
        return;
    beginEdit();
    const Tick start = tickAt(event.position.x);
    pattern->notes.push_back(
        {newId(), start, std::min(ppq / 2, pattern->length - start), pitchAt(event.position.y), 0.8f});
    dragNote_ = static_cast<int>(pattern->notes.size() - 1);
    const auto &note = pattern->notes.back();
    selectedNotes_ = {note.id.value};
    originalStart_ = note.start;
    originalDuration_ = note.duration;
    originalPitch_ = note.pitch;
    resizeNote_ = false;
    changed();
}
void StudioView::mouseDrag(const juce::MouseEvent &event)
{
    if (gainDrag_)
    {
        project_.tracks[static_cast<std::size_t>(selectedTrack_)].gain =
            juce::jlimit(0.0f, 1.0f, (event.position.x - 80) / 50);
        changed();
        return;
    }
    if (loopDrag_)
    {
        const Tick timeline = dragTimeline_;
        const auto tick =
            static_cast<Tick>(static_cast<double>(event.x - 150) / (getWidth() - 158) * timeline);
        project_.loopEnd = std::max(project_.loopStart + ppq * 4, (tick / (ppq * 4) + 1) * (ppq * 4));
        changed();
        return;
    }
    if (dragClip_ >= 0 && clipTrack_ >= 0)
    {
        beginEdit();
        auto &clip =
            project_.tracks[static_cast<std::size_t>(clipTrack_)].clips[static_cast<std::size_t>(dragClip_)];
        const Tick timeline = dragTimeline_;
        const Tick delta =
            static_cast<Tick>(std::round((event.position.x - dragOrigin_.x) / (getWidth() - 158) *
                                         static_cast<double>(timeline) / (ppq * 4))) *
            ppq * 4;
        if (resizeClip_)
            clip.length = std::max(ppq * 4, originalDuration_ + delta);
        else
            clip.start = std::max<Tick>(0, originalStart_ + delta);
        project_.loopEnd = std::max(project_.loopEnd, clip.start + clip.length);
        changed();
        return;
    }
    auto *pattern = selectedPattern();
    if (!pattern || dragNote_ < 0 || dragNote_ >= static_cast<int>(pattern->notes.size()))
        return;
    auto &note = pattern->notes[static_cast<std::size_t>(dragNote_)];
    if (velocityDrag_)
    {
        const auto grid = pianoGrid();
        const int y = grid.getBottom() + 12;
        const int height = std::max(10, editor().getBottom() - y - 22);
        note.velocity = juce::jlimit(0.05f, 1.0f, 1 - (event.position.y - y) / height);
        changed();
        return;
    }
    if (event.getDistanceFromDragStart() < 3)
        return;
    beginEdit();
    const auto grid = pianoGrid();
    const Tick delta = static_cast<Tick>(std::round((event.position.x - dragOrigin_.x) / grid.getWidth() *
                                                    static_cast<double>(pattern->length) / (ppq / 4))) *
                       (ppq / 4);
    if (resizeNote_)
        note.duration = juce::jlimit(std::min(ppq / 4, pattern->length - note.start),
                                     pattern->length - note.start, originalDuration_ + delta);
    else
    {
        const Tick nextStart = juce::jlimit<Tick>(0, pattern->length - note.duration, originalStart_ + delta);
        const int nextPitch =
            juce::jlimit(0, 127, originalPitch_ + pitchAt(event.position.y) - pitchAt(dragOrigin_.y));
        const Tick move = nextStart - note.start;
        const int pitchMove = nextPitch - note.pitch;
        for (auto &item : pattern->notes)
            if (selected(selectedNotes_, item))
            {
                item.start = juce::jlimit<Tick>(0, pattern->length - item.duration, item.start + move);
                item.pitch = juce::jlimit(0, 127, item.pitch + pitchMove);
            }
    }
    changed();
}
void StudioView::mouseUp(const juce::MouseEvent &)
{
    dragNote_ = dragClip_ = clipTrack_ = -1;
    gainDrag_ = loopDrag_ = velocityDrag_ = editStarted_ = false;
}
void StudioView::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel)
{
    editStarted_ = false;
    if (piano_ && pianoGrid().contains(event.getPosition()))
    {
        topPitch_ = juce::jlimit(24, 127, topPitch_ + (wheel.deltaY > 0 ? 2 : -2));
        repaint();
        return;
    }
    const auto e = editor();
    if (!piano_ && e.contains(event.getPosition()))
    {
        auto *current = selectedPattern();
        if (!current)
            return;
        for (int c = 0; c < static_cast<int>(project_.tracks.size()); ++c)
            for (int step = 0; step < current->length / (ppq / 4); ++step)
                if (stepCell(c, step).contains(event.getPosition()))
                    for (auto &note : patternForTrack(c)->notes)
                        if (note.start == step * ppq / 4)
                        {
                            beginEdit();
                            note.velocity = juce::jlimit(0.05f, 1.0f,
                                                         note.velocity + (wheel.deltaY > 0 ? 0.05f : -0.05f));
                            changed();
                            return;
                        }
    }
    if (arrangement().contains(event.getPosition()) && event.x < 150 && !project_.tracks.empty())
    {
        beginEdit();
        auto &track = project_.tracks[static_cast<std::size_t>(selectedTrack_)];
        if (event.mods.isShiftDown())
            track.pan = juce::jlimit(-1.0f, 1.0f, track.pan + (wheel.deltaY > 0 ? 0.1f : -0.1f));
        else
            track.gain = juce::jlimit(0.0f, 1.0f, track.gain + (wheel.deltaY > 0 ? 0.05f : -0.05f));
        changed();
    }
}
void StudioView::duplicateNotes(bool fromClipboard)
{
    auto *pattern = selectedPattern();
    if (!pattern)
        return;
    std::vector<Note> copies;
    if (fromClipboard)
        copies = clipboard_;
    else
        for (const auto &note : pattern->notes)
            if (selected(selectedNotes_, note))
                copies.push_back(note);
    if (copies.empty() || pattern->notes.size() + copies.size() > 512)
        return;
    std::erase_if(copies, [pattern](const Note &n) { return n.duration > pattern->length; });
    if (copies.empty())
        return;
    beginEdit();
    selectedNotes_.clear();
    for (auto copy : copies)
    {
        copy.id = newId();
        copy.start = std::min(pattern->length - copy.duration, copy.start + ppq / 4);
        selectedNotes_.push_back(copy.id.value);
        pattern->notes.push_back(std::move(copy));
    }
    changed();
}
bool StudioView::keyPressed(const juce::KeyPress &key)
{
    auto *pattern = selectedPattern();
    if (!pattern)
        return false;
    editStarted_ = false;
    if (key.getModifiers().isCommandDown())
    {
        if (key.getKeyCode() == 'A')
        {
            selectedNotes_.clear();
            for (const auto &n : pattern->notes)
                selectedNotes_.push_back(n.id.value);
            repaint();
            return true;
        }
        if (key.getKeyCode() == 'C')
        {
            clipboard_.clear();
            for (const auto &n : pattern->notes)
                if (selected(selectedNotes_, n))
                    clipboard_.push_back(n);
            return true;
        }
        if (key.getKeyCode() == 'D' || key.getKeyCode() == 'V')
        {
            duplicateNotes(key.getKeyCode() == 'V');
            return true;
        }
    }
    if ((key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) && !selectedNotes_.empty())
    {
        beginEdit();
        std::erase_if(pattern->notes, [this](const Note &n) { return selected(selectedNotes_, n); });
        selectedNotes_.clear();
        changed();
        return true;
    }
    return false;
}
juce::String StudioView::getTooltip()
{
    return "Space: play/stop. Arrangement: drag clips, right-click removes, Shift-click duplicates. "
           "M/S: mute/solo. Scroll track: gain, Shift-scroll: pan. Shift-drag ruler: loop. "
           "Piano roll: click adds, drag moves, right edge changes length. Ctrl+Z: undo.";
}
bool StudioView::exerciseEditing()
{
    const auto saved = project_;
    const int count = static_cast<int>(project_.patterns[0].notes.size());
    editStarted_ = false;
    toggleStep(0, 1);
    bool ok = static_cast<int>(project_.patterns[0].notes.size()) == count + 1;
    editStarted_ = false;
    toggleStep(0, 1);
    ok = ok && static_cast<int>(project_.patterns[0].notes.size()) == count;
    piano_ = true;
    selectedNotes_ = {project_.patterns[0].notes[0].id.value};
    editStarted_ = false;
    duplicateNotes();
    ok = ok && project_.patterns[0].notes.size() == saved.patterns[0].notes.size() + 1 &&
         validateProject(project_).empty();
    clipboard_ = {saved.patterns[0].notes[0]};
    clipboard_[0].pitch = 83;
    duplicateNotes(true);
    ok = ok && project_.patterns[0].notes.back().pitch == 83 && validateProject(project_).empty();
    clipboard_.clear();
    // Track references, rather than the order of the pattern array, choose the target.
    std::swap(project_.patterns[0], project_.patterns[1]);
    editStarted_ = false;
    const auto snareCount = project_.patterns[0].notes.size();
    toggleStep(0, 2);
    ok = ok && project_.patterns[0].notes.size() == snareCount;
    project_ = saved;
    resetSelection();
    piano_ = false;
    changed();
    return ok;
}
} // namespace opendaw
