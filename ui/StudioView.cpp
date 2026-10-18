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
    auto *pattern = const_cast<StudioView *>(this)->patternForTrack(channel);
    const int steps = pattern ? static_cast<int>(pattern->length / (ppq / 4)) : 16;
    const int width = getWidth() - 168;
    const int row = stepRowHeight();
    return {156 + step * width / steps + 2, editor().getY() + 64 + channel * row - stepScroll_,
            width / steps - 4, std::min(28, row - 8)};
}
int StudioView::stepRowHeight() const
{
    return juce::jlimit(24, 50,
                        (editor().getHeight() - 110) / std::max(1, static_cast<int>(project_.tracks.size())));
}
juce::Rectangle<int> StudioView::stepViewport() const
{
    return {8, editor().getY() + 60, getWidth() - 16, std::max(24, editor().getHeight() - 104)};
}
int StudioView::maxStepScroll() const
{
    return std::max(0, static_cast<int>(project_.tracks.size()) * stepRowHeight() -
                           stepViewport().getHeight() + 4);
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
    if (index == selectedTrack_ && !currentPattern_.value.empty())
        for (auto &pattern : project_.patterns)
            if (pattern.id == currentPattern_)
                return &pattern;
    auto &track = project_.tracks[static_cast<std::size_t>(index)];
    if (!track.patternId.value.empty())
        for (auto &pattern : project_.patterns)
            if (pattern.id == track.patternId)
                return &pattern;
    if (!track.clips.empty())
        for (auto &pattern : project_.patterns)
            if (pattern.id == track.clips[0].patternId)
                return &pattern;
    return &project_.patterns[static_cast<std::size_t>(index) % project_.patterns.size()];
}
void StudioView::focusPattern()
{
    if (const auto *pattern = selectedPattern(); pattern && !pattern->notes.empty())
    {
        int low = 127, high = 0;
        for (const auto &note : pattern->notes)
        {
            low = std::min(low, note.pitch);
            high = std::max(high, note.pitch);
        }
        const int rows = juce::jlimit(12, 24, pianoGrid().getHeight() / 12);
        topPitch_ = juce::jlimit(rows - 1, 127, high + std::max(0, rows - (high - low + 1)) / 2);
    }
}
void StudioView::resetSelection()
{
    selectedTrack_ = 0;
    selectedNotes_.clear();
    currentPattern_ = {};
    dragNote_ = dragClip_ = clipTrack_ = -1;
    gainDrag_ = loopDrag_ = velocityDrag_ = editStarted_ = false;
    topPitch_ = 66;
    pianoStart_ = arrangementStart_ = 0;
    pianoZoom_ = arrangementZoom_ = 1;
    stepScroll_ = 0;
}
Tick StudioView::barLength() const
{
    return ppq * project_.numerator * 4 / project_.denominator;
}
Tick StudioView::pianoSpan() const
{
    const auto *pattern = const_cast<StudioView *>(this)->selectedPattern();
    return pattern ? std::min(pattern->length, std::max<Tick>(snap_, pattern->length / pianoZoom_)) : ppq * 4;
}
Tick StudioView::arrangementTotal() const
{
    Tick end = std::max(barLength() * 8, project_.loopEnd);
    for (const auto &track : project_.tracks)
        for (const auto &clip : track.clips)
            end = std::max(end, clip.start + clip.length);
    return ((end + barLength() - 1) / barLength()) * barLength();
}
Tick StudioView::arrangementSpan() const
{
    return std::max(barLength(), arrangementTotal() / arrangementZoom_ / barLength() * barLength());
}
Tick StudioView::arrangementTick(float x) const
{
    return arrangementStart_ + static_cast<Tick>((x - 150) / (getWidth() - 158) * arrangementSpan());
}
void StudioView::zoomPiano(int direction, float anchor)
{
    const auto *pattern = selectedPattern();
    if (!pattern)
        return;
    const double fraction =
        juce::jlimit(0.0, 1.0, static_cast<double>(anchor - pianoGrid().getX()) / pianoGrid().getWidth());
    const double tick = pianoStart_ + fraction * pianoSpan();
    pianoZoom_ = juce::jlimit(1, 8, direction > 0 ? pianoZoom_ * 2 : pianoZoom_ / 2);
    pianoStart_ = juce::jlimit<Tick>(0, std::max<Tick>(0, pattern->length - pianoSpan()),
                                     static_cast<Tick>(tick - fraction * pianoSpan()));
    repaint();
}
void StudioView::zoomArrangement(int direction, float anchor)
{
    const double fraction = juce::jlimit(0.0, 1.0, static_cast<double>(anchor - 150) / (getWidth() - 158));
    const double tick = arrangementStart_ + fraction * arrangementSpan();
    arrangementZoom_ = juce::jlimit(1, 8, direction > 0 ? arrangementZoom_ * 2 : arrangementZoom_ / 2);
    arrangementStart_ = juce::jlimit<Tick>(0, arrangementTotal() - arrangementSpan(),
                                           static_cast<Tick>(tick - fraction * arrangementSpan()));
    repaint();
}
Tick StudioView::tickAt(float x) const
{
    auto *pattern = const_cast<StudioView *>(this)->selectedPattern();
    const auto grid = pianoGrid();
    if (!pattern)
        return 0;
    const auto raw = pianoStart_ + static_cast<double>(x - grid.getX()) / grid.getWidth() *
                                       static_cast<double>(pianoSpan());
    return juce::jlimit<Tick>(0, std::max<Tick>(0, pattern->length - snap_),
                              static_cast<Tick>(std::floor(raw / snap_)) * snap_);
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
        grid.getX() +
            static_cast<float>(note.start - pianoStart_) / static_cast<float>(pianoSpan()) * grid.getWidth(),
        grid.getY() + static_cast<float>(topPitch_ - note.pitch) * grid.getHeight() / rows + 1,
        std::max(3.0f,
                 static_cast<float>(note.duration) / static_cast<float>(pianoSpan()) * grid.getWidth() - 2),
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
    label(g, "Arrangement", {16, 5, 104, 26}, 16, t.text, true);
    g.setColour(project_.tracks.size() < 8 ? t.raised : t.surface);
    g.fillRoundedRectangle(120, 7, 24, 24, 4);
    g.setColour(project_.tracks.size() < 8 ? t.accent : t.muted);
    g.drawLine(126, 19, 138, 19, 1.5f);
    g.drawLine(132, 13, 132, 25, 1.5f);
    const Tick timeline = arrangementSpan();
    const float timeWidth = static_cast<float>(getWidth() - 158);
    const auto xAt = [this, timeWidth, timeline](Tick tick)
    {
        return 150.0f +
               static_cast<float>(tick - arrangementStart_) / static_cast<float>(timeline) * timeWidth;
    };
    const int rowHeight =
        project_.tracks.empty() ? 50 : (a.getHeight() - 40) / static_cast<int>(project_.tracks.size());
    const auto playTick = engine_.meters.positionTicks.load(std::memory_order_relaxed);
    const auto loopLeft = std::max(150.0f, xAt(project_.loopStart));
    const auto loopRight = std::min(static_cast<float>(getWidth() - 8), xAt(project_.loopEnd));
    g.setColour(t.accent.withAlpha(0.07f));
    g.fillRect(loopLeft, 33.0f, std::max(0.0f, loopRight - loopLeft), static_cast<float>(a.getHeight() - 36));
    const Tick major = std::max<Tick>(1, (timeline / barLength() + 7) / 8) * barLength();
    for (Tick tick = (arrangementStart_ / major + 1) * major - major; tick <= arrangementStart_ + timeline;
         tick += major)
    {
        const float x = xAt(tick);
        if (x < 150)
            continue;
        g.setColour(t.border.withAlpha(0.5f));
        g.drawVerticalLine(static_cast<int>(x), 32, static_cast<float>(a.getBottom() - 4));
        if (x < getWidth() - 35)
            label(g, juce::String(tick / barLength() + 1), {static_cast<int>(x) + 6, 7, 28, 18}, 10, t.muted);
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
        for (int button = 0; rowHeight >= 48 && button < 2; ++button)
        {
            const bool on = button == 0 ? track.mute : track.solo;
            g.setColour(on ? colour : t.raised);
            g.fillRoundedRectangle(18.0f + button * 27, static_cast<float>(y + 28), 23, 20, 3);
            label(g, button == 0 ? "M" : "S", {24 + button * 27, y + 28, 16, 20}, 11,
                  on ? t.background : t.muted);
        }
        if (rowHeight >= 48)
        {
            const float meter = engine_.meters.channelPeak[c].load(std::memory_order_relaxed);
            g.setColour(t.border);
            g.fillRoundedRectangle(80, static_cast<float>(y + 34), 53, 4, 2);
            g.setColour(colour);
            g.fillRoundedRectangle(80, static_cast<float>(y + 34), juce::jlimit(0.0f, 53.0f, meter * 130), 4,
                                   2);
            g.setColour(t.muted);
            g.fillEllipse(80 + track.gain * 50, static_cast<float>(y + 31), 8, 8);
        }
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
            g.reduceClipRegion(box.toNearestInt().reduced(2));
            if (rowHeight >= 40)
                for (const auto &pattern : project_.patterns)
                    if (pattern.id == clip.patternId)
                        for (Tick repeat = 0; repeat < clip.length; repeat += pattern.length)
                            for (const auto &note : pattern.notes)
                            {
                                if (repeat + note.start >= clip.length)
                                    continue;
                                const float nx = xAt(clip.start + repeat + note.start) + 3;
                                const float pitchSpacing =
                                    juce::jlimit(0.0f, 1.5f, (box.getHeight() - 38) / 12);
                                const float ny =
                                    box.getBottom() - 7 - static_cast<float>(note.pitch % 12) * pitchSpacing;
                                g.setColour(colour.withAlpha(0.85f));
                                g.fillRoundedRectangle(
                                    nx, ny,
                                    std::max(2.0f,
                                             static_cast<float>(note.duration) / timeline * timeWidth - 2),
                                    2.5f, 1);
                            }
            g.restoreState();
        }
    }
    if (playTick >= arrangementStart_ && playTick <= arrangementStart_ + timeline)
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
    const int patternNameX = piano_ ? 392 : 300;
    if (getWidth() > patternNameX + 520)
        label(g, juce::String(pattern->name),
              {patternNameX, e.getY() + 8, getWidth() - patternNameX - 280, 24}, 12, t.muted);
    if (piano_)
    {
        const auto button = quantizeButton();
        g.setColour(t.raised);
        g.fillRoundedRectangle(button.toFloat(), 4);
        label(g, "Quantize / Q", button, 11, selectedNotes_.empty() ? t.muted : t.accent);
        label(g, "Grid 1/" + juce::String(ppq * 4 / snap_), {getWidth() - 270, e.getY() + 8, 80, 24}, 11,
              t.muted);
        label(g, "Zoom " + juce::String(pianoZoom_) + "x", {getWidth() - 180, e.getY() + 8, 75, 24}, 11,
              t.muted);
    }
    label(g, juce::String(pattern->length / (ppq / 4)) + " steps", {getWidth() - 100, e.getY() + 8, 90, 24},
          12, t.accent);
    if (!piano_)
    {
        stepScroll_ = juce::jlimit(0, maxStepScroll(), stepScroll_);
        g.saveState();
        g.reduceClipRegion(stepViewport());
        for (std::size_t c = 0; c < project_.tracks.size(); ++c)
        {
            Pattern *p = patternForTrack(static_cast<int>(c));
            if (!p)
                continue;
            const int steps = static_cast<int>(p->length / (ppq / 4));
            const auto colour = trackColour(static_cast<int>(c), t);
            const auto labelBounds = stepCell(static_cast<int>(c), 0);
            label(g, juce::String(project_.tracks[c].name) + " / " + juce::String(steps),
                  {16, labelBounds.getY(), 132, labelBounds.getHeight()}, 12,
                  static_cast<int>(c) == selectedTrack_ ? t.accent : t.text);
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
        g.restoreState();
        label(g,
              maxStepScroll() > 0
                  ? "Scroll: browse tracks / Shift-scroll: velocity / +: add instrument"
                  : "Click: toggle / scroll: velocity / +: add instrument / right-click track: options",
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
    for (Tick tick = 0; tick <= pattern->length; tick += snap_)
    {
        const int x = grid.getX() + static_cast<int>(static_cast<double>(tick - pianoStart_) / pianoSpan() *
                                                     grid.getWidth());
        if (x < grid.getX() || x > grid.getRight())
            continue;
        g.setColour(t.border.withAlpha(tick % ppq == 0 ? 0.8f : 0.3f));
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
    const auto patternTick = playTick % pattern->length;
    if (engine_.meters.playing.load() && patternTick >= pianoStart_ &&
        patternTick < pianoStart_ + pianoSpan())
    {
        const auto x = grid.getX() + static_cast<int>(static_cast<double>(patternTick - pianoStart_) /
                                                      pianoSpan() * grid.getWidth());
        g.setColour(t.accent);
        g.drawVerticalLine(x, static_cast<float>(grid.getY()), static_cast<float>(grid.getBottom()));
    }
    const int velocityY = grid.getBottom() + 12, velocityH = std::max(10, e.getBottom() - velocityY - 22);
    label(g, "VEL", {10, velocityY, 40, 18}, 9, t.muted);
    for (const auto &note : pattern->notes)
    {
        const float x = noteBounds(note).getX() + 3;
        if (x < grid.getX() || x > grid.getRight())
            continue;
        g.setColour(selected(selectedNotes_, note) ? t.text : t.accent);
        g.drawLine(x, static_cast<float>(velocityY + velocityH), x,
                   static_cast<float>(velocityY) + (1 - note.velocity) * velocityH, 2);
    }
    label(g, "Click: add / right-click: delete / drag: move / edge: length / Shift: select / Q: quantize",
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
    dragTimeline_ = arrangementSpan();
    const auto a = arrangement(), e = editor();
    if (a.contains(event.getPosition()) && event.y < 34 && event.x < 150)
    {
        if (event.x >= 116 && project_.tracks.size() < 8)
            showAddTrackMenu(event.getScreenPosition());
        return;
    }
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
        if (piano_ && quantizeButton().contains(event.getPosition()))
        {
            quantizeSelection();
            return;
        }
        if (event.x > getWidth() - 110 && pattern)
        {
            beginEdit();
            pattern->length = pattern->length == ppq * 4 ? ppq * 8 : ppq * 4;
            std::erase_if(pattern->notes, [pattern](const Note &n) { return n.start >= pattern->length; });
            for (auto &n : pattern->notes)
                n.duration = std::min(n.duration, pattern->length - n.start);
            pianoStart_ = std::min(pianoStart_, std::max<Tick>(0, pattern->length - pianoSpan()));
            changed();
            return;
        }
        if (piano_ && event.x >= getWidth() - 270 && event.x < getWidth() - 190)
        {
            snap_ = snap_ == ppq / 4 ? ppq / 2 : snap_ == ppq / 2 ? ppq / 8 : ppq / 4;
            repaint();
            return;
        }
        if (piano_ && event.x >= getWidth() - 190)
        {
            if (pianoZoom_ == 8)
            {
                pianoZoom_ = 1;
                pianoStart_ = 0;
            }
            else
                zoomPiano(1, static_cast<float>(pianoGrid().getX()));
            repaint();
            return;
        }
    }
    if (a.contains(event.getPosition()))
    {
        const Tick timeline = dragTimeline_;
        const auto tick = arrangementTick(event.position.x);
        if (event.y < 34 && event.x >= 150)
        {
            if (event.mods.isShiftDown())
            {
                beginEdit();
                loopDrag_ = true;
                project_.loopStart = std::max<Tick>(0, tick / barLength() * barLength());
                project_.loopEnd = std::max(project_.loopEnd, project_.loopStart + barLength());
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
        if (selectedTrack_ != channel)
        {
            pianoStart_ = 0;
            currentPattern_ = {};
        }
        selectedTrack_ = channel;
        focusPattern();
        selectedNotes_.clear();
        auto &track = project_.tracks[static_cast<std::size_t>(channel)];
        const int localY = event.y - 36 - channel * rh;
        if (event.x < 150)
        {
            if (event.mods.isRightButtonDown())
            {
                showTrackMenu(channel, event.getScreenPosition());
                repaint();
                return;
            }
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
                currentPattern_ = clip.patternId;
                pianoStart_ = 0;
                focusPattern();
                repaint();
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
            const Tick start = std::max<Tick>(0, tick / barLength() * barLength());
            track.clips.push_back({newId(), selectedPattern()->id, start, barLength()});
            project_.loopEnd = std::max(project_.loopEnd, start + barLength());
            changed();
        }
        return;
    }
    if (!pattern)
        return;
    if (!piano_)
    {
        if (!stepViewport().contains(event.getPosition()))
            return;
        for (int channel = 0; channel < static_cast<int>(project_.tracks.size()); ++channel)
            for (int step = 0; step < patternForTrack(channel)->length / (ppq / 4); ++step)
                if (stepCell(channel, step).contains(event.getPosition()))
                {
                    if (selectedTrack_ != channel)
                    {
                        currentPattern_ = {};
                        selectedNotes_.clear();
                        pianoStart_ = 0;
                    }
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
        const auto tick = arrangementStart_ + static_cast<Tick>(static_cast<double>(event.x - 150) /
                                                                (getWidth() - 158) * timeline);
        project_.loopEnd = std::max(project_.loopStart + barLength(), (tick / barLength() + 1) * barLength());
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
                                         static_cast<double>(timeline) / barLength())) *
            barLength();
        if (resizeClip_)
            clip.length = std::max(barLength(), originalDuration_ + delta);
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
                                                    static_cast<double>(pianoSpan()) / snap_)) *
                       snap_;
    if (resizeNote_)
        note.duration = juce::jlimit(std::min(snap_, pattern->length - note.start),
                                     pattern->length - note.start, originalDuration_ + delta);
    else
    {
        const Tick nextStart = juce::jlimit<Tick>(0, pattern->length - note.duration, originalStart_ + delta);
        const int nextPitch =
            juce::jlimit(0, 127, originalPitch_ + pitchAt(event.position.y) - pitchAt(dragOrigin_.y));
        Tick move = nextStart - note.start;
        int pitchMove = nextPitch - note.pitch;
        for (const auto &item : pattern->notes)
            if (selected(selectedNotes_, item))
            {
                move = juce::jlimit<Tick>(-item.start, pattern->length - item.duration - item.start, move);
                pitchMove = juce::jlimit(-item.pitch, 127 - item.pitch, pitchMove);
            }
        for (auto &item : pattern->notes)
            if (selected(selectedNotes_, item))
            {
                item.start = juce::jlimit<Tick>(0, pattern->length - item.duration, item.start + move);
                item.pitch = juce::jlimit(0, 127, item.pitch + pitchMove);
            }
    }
    changed();
}
void StudioView::mouseUp(const juce::MouseEvent &event)
{
    if (dragClip_ >= 0 && clipTrack_ >= 0 && !resizeClip_ && editStarted_ &&
        arrangement().contains(event.getPosition()) && event.x >= 150 && event.y >= 36)
    {
        const int row =
            (arrangement().getHeight() - 40) / std::max(1, static_cast<int>(project_.tracks.size()));
        const int target = (event.y - 36) / row;
        if (target >= 0 && target < static_cast<int>(project_.tracks.size()) && target != clipTrack_)
        {
            auto &source = project_.tracks[static_cast<std::size_t>(clipTrack_)].clips;
            auto &destination = project_.tracks[static_cast<std::size_t>(target)].clips;
            if (dragClip_ < static_cast<int>(source.size()) && destination.size() < 256)
            {
                destination.push_back(source[static_cast<std::size_t>(dragClip_)]);
                source.erase(source.begin() + dragClip_);
                selectedTrack_ = target;
                changed();
            }
        }
    }
    dragNote_ = dragClip_ = clipTrack_ = -1;
    gainDrag_ = loopDrag_ = velocityDrag_ = editStarted_ = false;
}
void StudioView::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel)
{
    editStarted_ = false;
    if (piano_ && pianoGrid().contains(event.getPosition()))
    {
        if (event.mods.isCtrlDown())
            zoomPiano(wheel.deltaY > 0 ? 1 : -1, event.position.x);
        else if (event.mods.isShiftDown() || std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
        {
            const auto *pattern = selectedPattern();
            if (pattern)
                pianoStart_ = juce::jlimit<Tick>(0, std::max<Tick>(0, pattern->length - pianoSpan()),
                                                 pianoStart_ + (wheel.deltaY + wheel.deltaX > 0 ? -1 : 1) *
                                                                   std::max(snap_, pianoSpan() / 8));
        }
        else
            topPitch_ = juce::jlimit(juce::jlimit(12, 24, pianoGrid().getHeight() / 12) - 1, 127,
                                     topPitch_ + (wheel.deltaY > 0 ? 2 : -2));
        repaint();
        return;
    }
    if (arrangement().contains(event.getPosition()) && event.x >= 150)
    {
        if (event.mods.isCtrlDown())
            zoomArrangement(wheel.deltaY > 0 ? 1 : -1, event.position.x);
        else
            arrangementStart_ = juce::jlimit<Tick>(
                0, arrangementTotal() - arrangementSpan(),
                arrangementStart_ + (wheel.deltaY + wheel.deltaX > 0 ? -1 : 1) * barLength());
        repaint();
        return;
    }
    const auto e = editor();
    if (!piano_ && e.contains(event.getPosition()))
    {
        if (maxStepScroll() > 0 && !event.mods.isShiftDown())
        {
            stepScroll_ = juce::jlimit(0, maxStepScroll(), stepScroll_ + (wheel.deltaY > 0 ? -24 : 24));
            repaint();
            return;
        }
        if (!stepViewport().contains(event.getPosition()))
            return;
        auto *current = selectedPattern();
        if (!current)
            return;
        for (int c = 0; c < static_cast<int>(project_.tracks.size()); ++c)
            for (int step = 0; step < patternForTrack(c)->length / (ppq / 4); ++step)
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
        const int row = (arrangement().getHeight() - 40) / static_cast<int>(project_.tracks.size());
        const int hovered = (event.y - 36) / row;
        if (event.y < 36 || hovered < 0 || hovered >= static_cast<int>(project_.tracks.size()))
            return;
        beginEdit();
        auto &track = project_.tracks[static_cast<std::size_t>(hovered)];
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
juce::Rectangle<int> StudioView::quantizeButton() const
{
    return {286, editor().getY() + 8, 90, 24};
}
void StudioView::quantizeSelection()
{
    auto *pattern = selectedPattern();
    if (!pattern)
        return;
    auto candidate = *pattern;
    if (!quantizeNoteStarts(candidate, selectedNotes_, snap_))
        return;
    editStarted_ = false;
    beginEdit();
    *pattern = std::move(candidate);
    changed();
}
bool StudioView::keyPressed(const juce::KeyPress &key)
{
    auto *pattern = selectedPattern();
    if (!pattern)
        return false;
    editStarted_ = false;
    if (piano_ && key.getKeyCode() == 'Q' && !key.getModifiers().isAnyModifierKeyDown())
    {
        quantizeSelection();
        return true;
    }
    if (key.getModifiers().isCommandDown())
    {
        if (key.getKeyCode() == 'T')
        {
            createTrack(Track::Instrument::sine);
            return true;
        }
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
    if (piano_ && !selectedNotes_.empty() && !key.getModifiers().isCommandDown())
    {
        Tick move = 0, length = 0;
        int transpose = 0;
        const bool shift = key.getModifiers().isShiftDown();
        if (key.getKeyCode() == juce::KeyPress::leftKey)
            shift ? length = -snap_ : move = -snap_;
        else if (key.getKeyCode() == juce::KeyPress::rightKey)
            shift ? length = snap_ : move = snap_;
        else if (key.getKeyCode() == juce::KeyPress::upKey)
            transpose = shift ? 12 : 1;
        else if (key.getKeyCode() == juce::KeyPress::downKey)
            transpose = shift ? -12 : -1;
        else
            return false;
        for (const auto &note : pattern->notes)
            if (selected(selectedNotes_, note))
            {
                move = juce::jlimit<Tick>(-note.start, pattern->length - note.start - note.duration, move);
                length = juce::jlimit<Tick>(std::min(snap_, note.duration) - note.duration,
                                            pattern->length - note.start - note.duration, length);
                transpose = juce::jlimit(-note.pitch, 127 - note.pitch, transpose);
            }
        if (move == 0 && length == 0 && transpose == 0)
            return true;
        beginEdit();
        for (auto &note : pattern->notes)
            if (selected(selectedNotes_, note))
            {
                note.start += move;
                note.duration += length;
                note.pitch += transpose;
            }
        changed();
        return true;
    }
    return false;
}
void StudioView::applyTrackEdit(std::function<EditResult(Project &)> edit)
{
    auto candidate = project_;
    const auto result = edit(candidate);
    if (!result.error.empty())
    {
        if (onError)
            onError(juce::String(result.error));
        return;
    }
    editStarted_ = false;
    beginEdit();
    project_ = std::move(candidate);
    resetSelection();
    for (std::size_t i = 0; i < project_.tracks.size(); ++i)
        if (project_.tracks[i].id == result.trackId)
            selectedTrack_ = static_cast<int>(i);
    focusPattern();
    stepScroll_ = juce::jlimit(0, maxStepScroll(),
                               (selectedTrack_ + 1) * stepRowHeight() - stepViewport().getHeight() + 4);
    changed();
}
void StudioView::createTrack(Track::Instrument instrument)
{
    applyTrackEdit([instrument](Project &project) { return addInstrumentTrack(project, instrument); });
}
void StudioView::showAddTrackMenu(juce::Point<int> screen)
{
    juce::PopupMenu menu;
    menu.addSectionHeader("Add an instrument");
    menu.addItem(1, "Kick");
    menu.addItem(2, "Snare");
    menu.addItem(3, "Closed hat");
    menu.addItem(4, "Sine synth");
    menu.addItem(5, "Triangle bass");
    const juce::Component::SafePointer<StudioView> safe(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea({screen.x, screen.y, 1, 1}),
        [safe](int result)
        {
            if (safe && result >= 1 && result <= 5)
                safe->createTrack(static_cast<Track::Instrument>(result - 1));
        });
}
void StudioView::showTrackMenu(int track, juce::Point<int> screen)
{
    if (track < 0 || track >= static_cast<int>(project_.tracks.size()))
        return;
    const auto id = project_.tracks[static_cast<std::size_t>(track)].id;
    juce::PopupMenu menu;
    menu.addSectionHeader(juce::String(project_.tracks[static_cast<std::size_t>(track)].name));
    menu.addItem(1, "Rename track...");
    menu.addItem(2, "Duplicate track", project_.tracks.size() < 8);
    menu.addItem(3, "Delete track");
    const juce::Component::SafePointer<StudioView> safe(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea({screen.x, screen.y, 1, 1}),
        [safe, id](int result)
        {
            if (!safe)
                return;
            if (result == 1)
                safe->showRenameTrack(id);
            else if (result == 2)
                safe->applyTrackEdit([id](Project &project) { return duplicateTrack(project, id); });
            else if (result == 3)
                safe->applyTrackEdit([id](Project &project) { return removeTrack(project, id); });
        });
}
void StudioView::showRenameTrack(const EntityId &id)
{
    const auto track = std::find_if(project_.tracks.begin(), project_.tracks.end(),
                                    [&id](const Track &item) { return item.id == id; });
    if (track == project_.tracks.end())
        return;
    auto *dialog = new juce::AlertWindow("Name this track", "Choose a name for your instrument.",
                                         juce::MessageBoxIconType::NoIcon, this);
    dialog->addTextEditor("name", juce::String(track->name), "Track name");
    dialog->getTextEditor("name")->setInputRestrictions(64);
    dialog->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    const juce::Component::SafePointer<StudioView> safe(this);
    const juce::Component::SafePointer<juce::AlertWindow> safeDialog(dialog);
    dialog->enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [safe, safeDialog, id](int result)
                                {
                                    if (result == 1 && safe && safeDialog)
                                    {
                                        const auto name =
                                            safeDialog->getTextEditorContents("name").trim().toStdString();
                                        safe->applyTrackEdit([id, name](Project &project)
                                                             { return renameTrack(project, id, name); });
                                    }
                                }),
                            true);
}
juce::String StudioView::getTooltip()
{
    return "Space: play/stop. + or Ctrl+T: add track; right-click a track name for rename/duplicate/delete. "
           "Arrangement: drag clips, right-click removes, Shift-click duplicates. "
           "M/S: mute/solo. Scroll track: gain, Shift-scroll: pan. Shift-drag ruler: loop. "
           "Ctrl-wheel: zoom. Shift-wheel: horizontal scroll. Piano roll: click adds, drag moves, "
           "right edge changes length. Q: quantize selected notes to Grid. "
           "Arrows: move notes; Shift+arrows: length/octave. "
           "Ctrl+Z: undo. F2: session name. F3: mixer.";
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
    const auto beforeNudge = project_.patterns[0].notes.back();
    keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
    keyPressed(
        juce::KeyPress(juce::KeyPress::upKey, juce::ModifierKeys(juce::ModifierKeys::shiftModifier), 0));
    ok = ok && project_.patterns[0].notes.back().start == beforeNudge.start + snap_ &&
         project_.patterns[0].notes.back().pitch == 95 && validateProject(project_).empty();
    clipboard_.clear();
    zoomPiano(1, static_cast<float>(pianoGrid().getX()));
    ok = ok && pianoZoom_ == 2 && pianoSpan() == selectedPattern()->length / 2;
    pianoStart_ = ppq;
    ok = ok && tickAt(static_cast<float>(pianoGrid().getX())) == ppq;
    zoomArrangement(1, 150);
    ok = ok && arrangementZoom_ == 2 && arrangementTick(150) == 0 &&
         arrangementSpan() == arrangementTotal() / 2;
    // Track references, rather than the order of the pattern array, choose the target.
    std::swap(project_.patterns[0], project_.patterns[1]);
    editStarted_ = false;
    const auto snareCount = project_.patterns[0].notes.size();
    toggleStep(0, 2);
    ok = ok && project_.patterns[0].notes.size() == snareCount;
    project_ = saved;
    resetSelection();
    createTrack(Track::Instrument::sine);
    ok = ok && project_.tracks.size() == saved.tracks.size() + 1;
    const auto addedId = project_.tracks.back().id;
    applyTrackEdit([addedId](Project &project) { return renameTrack(project, addedId, "New lead"); });
    ok = ok && project_.tracks.back().name == "New lead";
    applyTrackEdit([addedId](Project &project) { return duplicateTrack(project, addedId); });
    ok = ok && project_.tracks.size() == saved.tracks.size() + 2;
    applyTrackEdit([addedId](Project &project) { return removeTrack(project, addedId); });
    for (int attempt = 0; attempt < 8 && project_.tracks.size() < 8; ++attempt)
        createTrack(Track::Instrument::hat);
    ok = ok && project_.tracks.size() == 8 && validateProject(project_).empty();
    const auto bounds = getBounds();
    setSize(662, 488);
    ok = ok && quantizeButton().getRight() < getWidth() - 270 && editor().contains(quantizeButton());
    stepScroll_ = maxStepScroll();
    ok = ok && maxStepScroll() > 0 && stepViewport().contains(stepCell(7, 0));
    project_.tracks.back().clips.clear();
    ok = ok && selectedPattern() && selectedPattern()->id == project_.tracks.back().patternId;
    const auto patternCount = project_.patterns.size();
    createTrack(Track::Instrument::sine);
    ok = ok && project_.tracks.size() == 8 && project_.patterns.size() == patternCount;
    setBounds(bounds);
    project_ = saved;
    resetSelection();
    piano_ = false;
    changed();
    return ok;
}
} // namespace opendaw
