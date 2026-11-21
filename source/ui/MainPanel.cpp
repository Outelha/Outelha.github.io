#include "MainPanel.h"

#include "Hardware.h"
#include "PluginProcessor.h"

namespace sfx::ui
{
using namespace juce;

namespace
{
// Short labels for the compact chain readout.
const char* shortName (EffectId id)
{
    switch (id)
    {
        case EffectId::saturation: return "SAT";
        case EffectId::resampler: return "RESAMP";
        case EffectId::tape: return "TAPE";
        case EffectId::chorus: return "CHORUS";
        case EffectId::tremolo: return "TREM";
        case EffectId::reverse: return "REV";
        case EffectId::delay: return "DELAY";
        case EffectId::reverb: return "VERB";
    }
    return "";
}

// Layout in view coordinates (1260 x 840 design); the panel itself starts at topOffset.
Rectangle<int> V (int x, int y, int w, int h) { return { x, y - MainPanel::topOffset, w, h }; }

const auto hardwarePlate = V (22, 96, 1216, 386);
const auto driftModule = V (104, 108, 254, 268);
const auto unstableModule = V (902, 108, 254, 268);
const auto driftTubeArea = V (58, 130, 30, 164);
const auto unstableTubeArea = V (1172, 130, 30, 164);
const auto displayArea = V (394, 104, 472, 198);
const auto vibeTray = V (364, 322, 532, 80);
const auto generateArea = V (480, 414, 300, 52);
const auto chainPlate = V (18, 494, 1224, 334);
const auto chainTray = V (40, 528, 938, 282);
const auto masterTray = V (996, 528, 222, 248);

constexpr int tileW = 104, tileH = 214;

// Visible in the editor's hierarchy. (isShowing() also needs a native window, which a
// headless snapshot render does not have; VBlank callbacks already stop when hidden.)
bool visibleInHierarchy (const Component& c)
{
    // The top-level editor itself is made visible by the host window; only its children count.
    for (auto* p = &c; p != nullptr && p->getParentComponent() != nullptr; p = p->getParentComponent())
        if (! p->isVisible())
            return false;
    return true;
}

float normalised (const AudioProcessorValueTreeState& s, const char* id)
{
    if (auto* p = s.getParameter (id))
        return p->getValue();
    return 0.0f;
}

float raw (const AudioProcessorValueTreeState& s, const char* id)
{
    if (auto* v = s.getRawParameterValue (id))
        return v->load();
    return 0.0f;
}

// Signal level (0..1 on a 48 dB scale) from the newest few segments of the visual feed.
float feedLevel (const VisualFeed& feed)
{
    std::array<float, 12> recent {};
    feed.read (recent.data(), static_cast<std::uint32_t> (recent.size()));
    const auto peak = *std::max_element (recent.begin(), recent.end());
    return jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (peak, -60.0f) + 48.0f) / 48.0f);
}

// Machined module housing with chamfered corners (Drift / Unstable).
void drawModulePlate (Graphics& g, Rectangle<float> r)
{
    const auto ch = 26.0f;
    Path p;
    p.startNewSubPath (r.getX() + ch, r.getY());
    p.lineTo (r.getRight() - ch, r.getY());
    p.lineTo (r.getRight(), r.getY() + ch);
    p.lineTo (r.getRight(), r.getBottom() - ch);
    p.lineTo (r.getRight() - ch, r.getBottom());
    p.lineTo (r.getX() + ch, r.getBottom());
    p.lineTo (r.getX(), r.getBottom() - ch);
    p.lineTo (r.getX(), r.getY() + ch);
    p.closeSubPath();

    for (int i = 0; i < 4; ++i)
    {
        g.setColour (Colours::black.withAlpha (0.16f));
        g.fillPath (p, AffineTransform::translation (0.0f, 2.0f + static_cast<float> (i) * 1.6f));
    }
    g.setGradientFill (ColourGradient (Colour (0xff6a6b70), r.getX(), r.getY(), Colour (0xff1a1b1e), r.getRight(), r.getBottom(), false));
    g.fillPath (p);
    const auto inner = r.reduced (3.0f);
    Path in;
    in.startNewSubPath (inner.getX() + ch - 1.0f, inner.getY());
    in.lineTo (inner.getRight() - ch + 1.0f, inner.getY());
    in.lineTo (inner.getRight(), inner.getY() + ch - 1.0f);
    in.lineTo (inner.getRight(), inner.getBottom() - ch + 1.0f);
    in.lineTo (inner.getRight() - ch + 1.0f, inner.getBottom());
    in.lineTo (inner.getX() + ch - 1.0f, inner.getBottom());
    in.lineTo (inner.getX(), inner.getBottom() - ch + 1.0f);
    in.lineTo (inner.getX(), inner.getY() + ch - 1.0f);
    in.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0xff3b3c41), inner.getX(), inner.getY(), Colour (0xff17181b), inner.getRight(), inner.getBottom(), false));
    g.fillPath (in);
    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (in);
        g.setTiledImageFill (hw::brushedTexture(), 0, 0, 0.5f);
        g.fillPath (in);
        ColourGradient sheen (Colours::white.withAlpha (0.10f), inner.getX(), inner.getY(), Colours::transparentWhite,
                              inner.getCentreX(), inner.getCentreY(), false);
        g.setGradientFill (sheen);
        g.fillPath (in);
    }
    g.setColour (Colours::white.withAlpha (0.22f));
    g.strokePath (in, PathStrokeType (0.8f));
    g.setColour (Colours::black.withAlpha (0.85f));
    g.strokePath (p, PathStrokeType (1.0f));
    const auto si = 16.0f;
    for (auto c : { r.getTopLeft().translated (si + 6.0f, si), r.getTopRight().translated (-si - 6.0f, si),
                    r.getBottomLeft().translated (si + 6.0f, -si), r.getBottomRight().translated (-si - 6.0f, -si) })
        hw::drawScrew (g, c, 4.2f, c.x * 0.02f);
}

void drawSectionLabel (Graphics& g, const String& text, Rectangle<float> r, Justification j = Justification::centredLeft)
{
    hw::drawEngraved (g, text, r, font (Type::label, 17.0f, 0.2f), j, Colour (0xffd3cbbd));
}
} // namespace

//==============================================================================
Path KeyCap::iconPath (Icon icon, Rectangle<float> r)
{
    Path p;
    const auto c = r.getCentre();
    const auto s = jmin (r.getWidth(), r.getHeight());
    switch (icon)
    {
        case Icon::cassette:
        {
            const auto b = Rectangle<float> (s * 1.35f, s * 0.86f).withCentre (c);
            p.addRoundedRectangle (b, s * 0.08f);
            const auto win = b.reduced (b.getWidth() * 0.16f, b.getHeight() * 0.22f).withTrimmedBottom (b.getHeight() * 0.18f);
            p.addRoundedRectangle (win, s * 0.05f);
            for (auto fx : { 0.3f, 0.7f })
                p.addEllipse (Rectangle<float> (s * 0.16f, s * 0.16f).withCentre ({ win.getX() + win.getWidth() * fx, win.getCentreY() }));
            p.startNewSubPath (b.getX() + b.getWidth() * 0.25f, b.getBottom());
            p.lineTo (b.getX() + b.getWidth() * 0.32f, b.getBottom() - b.getHeight() * 0.2f);
            p.lineTo (b.getRight() - b.getWidth() * 0.32f, b.getBottom() - b.getHeight() * 0.2f);
            p.lineTo (b.getRight() - b.getWidth() * 0.25f, b.getBottom());
            break;
        }
        case Icon::cloud:
        {
            const auto w = s * 1.3f, r1 = s * 0.24f, r2 = s * 0.34f;
            const auto yb = c.y + s * 0.28f;
            const auto xl = c.x - w * 0.5f + r1, xr = c.x + w * 0.5f - r1;
            p.startNewSubPath (xl, yb);
            p.addCentredArc (xl, yb - r1, r1, r1, 0.0f, MathConstants<float>::pi, MathConstants<float>::twoPi);
            p.addCentredArc (c.x - s * 0.04f, yb - r1 - s * 0.12f, r2, r2, 0.0f, -1.25f, 1.35f);
            p.addCentredArc (xr, yb - r1, r1, r1, 0.0f, 0.0f, MathConstants<float>::pi);
            p.closeSubPath();
            break;
        }
        case Icon::moon:
        {
            Path a;
            p.addEllipse (Rectangle<float> (s * 0.9f, s * 0.9f).withCentre (c));
            p.addEllipse (Rectangle<float> (s * 0.78f, s * 0.78f).withCentre (c.translated (s * 0.28f, -s * 0.2f)));
            p.setUsingNonZeroWinding (false);
            break;
        }
        case Icon::waveform:
        {
            static constexpr float heights[] = { 0.25f, 0.55f, 0.35f, 0.9f, 0.6f, 1.0f, 0.45f, 0.75f, 0.3f };
            const auto n = static_cast<int> (std::size (heights));
            const auto step = s * 1.2f / static_cast<float> (n);
            for (int i = 0; i < n; ++i)
            {
                const auto x = c.x - s * 0.6f + step * (static_cast<float> (i) + 0.5f);
                const auto h = s * 0.85f * heights[i];
                p.startNewSubPath (x, c.y - h * 0.5f);
                p.lineTo (x, c.y + h * 0.5f);
            }
            break;
        }
        case Icon::planet:
        {
            p.addEllipse (Rectangle<float> (s * 0.6f, s * 0.6f).withCentre (c));
            Path ring;
            ring.addEllipse (Rectangle<float> (s * 1.3f, s * 0.36f).withCentre (c));
            p.addPath (ring, AffineTransform::rotation (-0.45f, c.x, c.y));
            break;
        }
        case Icon::flask:
        {
            const auto top = c.y - s * 0.48f, bottom = c.y + s * 0.45f;
            p.startNewSubPath (c.x - s * 0.14f, top);
            p.lineTo (c.x - s * 0.14f, c.y - s * 0.08f);
            p.lineTo (c.x - s * 0.42f, bottom);
            p.lineTo (c.x + s * 0.42f, bottom);
            p.lineTo (c.x + s * 0.14f, c.y - s * 0.08f);
            p.lineTo (c.x + s * 0.14f, top);
            p.startNewSubPath (c.x - s * 0.22f, top);
            p.lineTo (c.x + s * 0.22f, top);
            p.startNewSubPath (c.x - s * 0.27f, c.y + s * 0.22f);
            p.lineTo (c.x + s * 0.27f, c.y + s * 0.22f);
            break;
        }
        case Icon::chevrons:
        {
            for (int i = 0; i < 3; ++i)
            {
                const auto x = r.getX() + static_cast<float> (i) * s * 0.42f;
                p.startNewSubPath (x, c.y - s * 0.38f);
                p.lineTo (x + s * 0.3f, c.y);
                p.lineTo (x, c.y + s * 0.38f);
            }
            break;
        }
        case Icon::none:
            break;
    }
    return p;
}

KeyCap::KeyCap (const String& label, Colour accentColour, bool isLarge) : Button (label), accent (accentColour), large (isLarge)
{
    setMouseCursor (MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus (false);
}

void KeyCap::paintButton (Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (large ? 9.0f : 4.0f, large ? 7.0f : 4.0f);
    const bool on = lit || getToggleState();

    if (large)
    {
        hw::KeyLook key;
        key.accent = accent;
        key.lit = true;
        key.over = over;
        key.down = down;
        key.corner = 10.0f;
        hw::drawKey (g, b, key);
        auto text = b.translated (0.0f, down ? 0.8f : 0.0f);
        const auto f = hasLabelFont ? labelFont : font (Type::labelBold, b.getHeight() * 0.52f, 0.22f);
        const auto label = getButtonText().toUpperCase();
        hw::drawKeyText (g, label, text, f, true, accent);
        if (icon == Icon::chevrons)
        {
            const auto textW = GlyphArrangement::getStringWidth (f, label);
            const auto ch = b.getHeight() * 0.36f;
            for (int side = 0; side < 2; ++side)
            {
                const auto x = side == 0 ? text.getCentreX() - textW * 0.5f - ch * 2.2f : text.getCentreX() + textW * 0.5f + ch * 1.15f;
                auto chev = iconPath (Icon::chevrons, Rectangle<float> (ch * 1.2f, ch).withPosition (x, text.getCentreY() - ch * 0.5f));
                if (side == 1)
                    chev.applyTransform (AffineTransform::scale (-1.0f, 1.0f, x + ch * 0.6f, 0.0f));
                g.setColour (Colour (0xff5a1e00).withAlpha (0.6f));
                g.strokePath (chev, PathStrokeType (ch * 0.16f, PathStrokeType::mitered, PathStrokeType::rounded), AffineTransform::translation (0.0f, 1.0f));
                g.setColour (Colour (0xfffff1dc));
                g.strokePath (chev, PathStrokeType (ch * 0.16f, PathStrokeType::mitered, PathStrokeType::rounded));
            }
        }
        return;
    }

    hw::KeyLook key;
    key.accent = accent;
    key.over = over;
    key.down = down;
    key.corner = 8.0f;
    if (on)
        hw::drawRimGlow (g, b, 8.0f, accent, 7.0f, 0.9f);
    hw::drawKey (g, b, key);
    if (on)
    {
        // Lit vibe key: warm light from inside, orange rim.
        hw::drawLightSpill (g, b.reduced (4.0f).translated (0.0f, b.getHeight() * 0.1f), accent, 0.55f);
        g.setColour (accent);
        g.drawRoundedRectangle (b.reduced (1.0f), 8.0f, 1.8f);
    }

    auto content = b.reduced (4.0f, 6.0f).translated (0.0f, down ? 0.8f : 0.0f);
    const auto ink = on ? Colour (0xfffff4e6) : Colour (0xffdedad2);
    if (icon != Icon::none)
    {
        auto iconArea = content.removeFromTop (content.getHeight() * 0.5f).reduced (0.0f, 2.0f);
        const auto path = iconPath (icon, iconArea.withSizeKeepingCentre (iconArea.getHeight(), iconArea.getHeight()));
        const auto stroke = PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded);
        g.setColour (Colours::black.withAlpha (0.6f));
        if (icon == Icon::moon)
            g.fillPath (path, AffineTransform::translation (0.0f, 1.0f));
        else
            g.strokePath (path, stroke, AffineTransform::translation (0.0f, 1.0f));
        g.setColour (ink);
        if (icon == Icon::moon)
            g.fillPath (path);
        else
            g.strokePath (path, stroke);
    }
    auto f = hasLabelFont ? labelFont : font (Type::labelBold, 19.0f, 0.0f);
    const auto label = getButtonText().toUpperCase();
    const auto width = GlyphArrangement::getStringWidth (f, label);
    if (width > content.getWidth())
        f = f.withHorizontalScale (jmax (0.72f, content.getWidth() / width));
    g.setFont (f);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawText (label, content.translated (0.0f, 1.0f), Justification::centred, false);
    g.setColour (ink);
    g.drawText (label, content, Justification::centred, false);
}

//==============================================================================
EffectTile::EffectTile (AudioProcessorValueTreeState& s, EffectId id, FrameClock& c, const ShobeatsAudioProcessor& p)
    : Button (infoFor (id).name), state (s), processor (p), effect (id), clock (c)
{
    setClickingTogglesState (true);
    setTooltip (String (infoFor (id).name) + ": click to switch on/off, double-click to open the pedal");
    attachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (state, infoFor (id).enabledId, *this);
    setMouseCursor (MouseCursor::PointingHandCursor);
    readState (state, effect, shown);
    clock.add (*this);
}

EffectTile::~EffectTile()
{
    clock.remove (*this);
}

void EffectTile::readState (const AudioProcessorValueTreeState& s, EffectId id, pedal::State& out)
{
    out.enabled = raw (s, infoFor (id).enabledId) > 0.5f;
    const auto knobs = pedal::faceKnobs (id);
    const bool synced = knobs.syncId != nullptr && raw (s, knobs.syncId) > 0.5f;
    out.knobA = normalised (s, synced ? knobs.divisionId : knobs.a);
    out.knobB = normalised (s, knobs.b);
    out.extra = {};
    switch (id)
    {
        case EffectId::saturation: out.extra[0] = normalised (s, ParamID::satDrive); break;
        case EffectId::resampler: out.extra[1] = normalised (s, ParamID::rsBits); break;
        case EffectId::chorus:
            out.extra[0] = normalised (s, ParamID::chorusRate);
            out.extra[1] = normalised (s, ParamID::chorusDepth);
            break;
        case EffectId::tremolo:
            out.extra[0] = normalised (s, ParamID::tremRate);
            out.extra[1] = normalised (s, ParamID::tremDepth);
            out.extra[2] = raw (s, ParamID::tremShape);
            break;
        case EffectId::delay: out.extra[1] = normalised (s, ParamID::delayFeedback); break;
        case EffectId::reverb: out.extra[0] = 0.6f * normalised (s, ParamID::reverbDecay) + 0.4f * normalised (s, ParamID::reverbMix); break;
        case EffectId::tape:
        case EffectId::reverse:
            break;
    }
}

bool EffectTile::frame (double, double dt)
{
    if (! visibleInHierarchy (*this))
        return false;

    pedal::State next = shown;
    readState (state, effect, next);

    // Audio is running when the processor keeps consuming samples.
    const auto samples = processor.getVisualFeed().samplesProcessed();
    const bool running = samples != lastSamples;
    lastSamples = samples;

    bool animating = false;
    const auto newLevel = next.enabled && running ? feedLevel (processor.getVisualFeed()) : 0.0f;
    level += (newLevel - level) * static_cast<float> (jmin (1.0, dt * (newLevel > level ? 20.0 : 6.0)));
    next.level = level;
    if (effect == EffectId::tape || effect == EffectId::resampler)
        animating = animating || std::abs (level - shown.level) > 0.002f;

    if (effect == EffectId::tape && next.enabled && running)
    {
        reelPhase += static_cast<float> (dt) * 2.4f;
        animating = true;
    }
    next.reelPhase = reelPhase;

    if ((effect == EffectId::chorus || effect == EffectId::tremolo) && next.enabled)
    {
        animTime += dt;
        animating = true;
    }
    next.time = animTime;

    const bool changed = next.enabled != shown.enabled || ! exactlyEqual (next.knobA, shown.knobA) || ! exactlyEqual (next.knobB, shown.knobB)
                         || next.extra != shown.extra;
    shown = next;
    if (animating || changed)
        repaint();
    return animating;
}

void EffectTile::mouseDoubleClick (const MouseEvent&)
{
    // The first click of the double-click toggled the pedal; undo that before opening it.
    setToggleState (! getToggleState(), sendNotificationSync);
    if (onOpen)
        onOpen (effect);
}

void EffectTile::paintButton (Graphics& g, bool over, bool down)
{
    auto area = getLocalBounds().toFloat();
    const auto badge = area.removeFromBottom (24.0f);
    auto body = area.reduced (2.0f, 4.0f);
    if (down)
        body.translate (0.0f, 1.0f);

    pedal::State s = shown;
    s.enabled = getToggleState();
    s.hover = over;
    s.captions = false;
    s.selected = false;
    pedal::paint (g, effect, pedal::layout (effect, body, false), s);
    pedal::paintSlotBadge (g, badge.withSizeKeepingCentre (34.0f, 21.0f).translated (0.0f, 2.0f), slot, false, accentColour (effect));
}

//==============================================================================
CentralDisplay::CentralDisplay (ShobeatsAudioProcessor& p, FrameClock& c) : processor (p), clock (c)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (10);
    timerCallback();
    clock.add (*this);
}

CentralDisplay::~CentralDisplay()
{
    clock.remove (*this);
}

Rectangle<float> CentralDisplay::screenArea() const
{
    return getLocalBounds().toFloat().reduced (16.0f, 14.0f);
}

Rectangle<float> CentralDisplay::waveArea() const
{
    const auto s = screenArea().reduced (18.0f, 12.0f);
    return { s.getX(), s.getY() + 78.0f, s.getWidth(), s.getHeight() - 78.0f - 22.0f };
}

void CentralDisplay::resized() {}

void CentralDisplay::timerCallback()
{
    const auto [n, c] = processor.getCurrentPresetLabel();
    String desc;
    auto& pm = processor.getPresetManager();
    if (pm.getCurrentIndex() >= 0)
        desc = pm.getPresets()[static_cast<size_t> (pm.getCurrentIndex())].description;
    else if (pm.getLastGenerated() && pm.getLastGenerated()->name == n)
        desc = pm.getLastGenerated()->description;
    const auto v = Generator::vibeNames()[static_cast<int> (pm.getSelectedVibe())];

    std::array<bool, numEffects> on {};
    for (int i = 0; i < numEffects; ++i)
        on[static_cast<size_t> (i)] = processor.getState().getRawParameterValue (effectInfo[static_cast<size_t> (i)].enabledId)->load() > 0.5f;
    const auto o = processor.getChainOrder().get();
    const auto d = processor.getState().getRawParameterValue (ParamID::drift)->load();
    const auto u = processor.getState().getRawParameterValue (ParamID::unstable)->load();
    const auto t = processor.getCurrentBpm();
    const auto host = processor.isTempoFromHost();

    const auto changed = [] (auto a, auto b) { return ! juce::exactlyEqual (a, b); };
    if (n != name || c != category || desc != description || v != vibe || on != active || o != order || changed (d, drift) || changed (u, unstable)
        || changed (t, bpm) || host != hostTempo)
    {
        name = n;
        category = c;
        description = desc;
        vibe = v;
        active = on;
        order = o;
        drift = d;
        unstable = u;
        bpm = t;
        hostTempo = host;
        repaint();
    }
}

bool CentralDisplay::frame (double, double)
{
    if (! visibleInHierarchy (*this))
        return false;
    const auto written = processor.getVisualFeed().read (wave.data(), static_cast<std::uint32_t> (wave.size()));
    if (written == lastWritten)
        return false;
    lastWritten = written;
    repaint (waveArea().expanded (4.0f).getSmallestIntegerContainer());
    return true;
}

void CentralDisplay::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto amber = Colour (0xffffa53a);
    const auto orange = Colour (0xffff8a1f);

    // Heavy bezel: light spill behind, chamfered chrome frame, black glass.
    hw::drawLightSpill (g, bounds.reduced (20.0f, 0.0f).withTrimmedTop (bounds.getHeight() * 0.6f).translated (0.0f, 18.0f), orange, 0.55f);
    const auto frame = bounds.reduced (4.0f);
    hw::drawRimGlow (g, frame, 18.0f, orange, 6.0f, 0.45f);
    g.setGradientFill (ColourGradient (Colour (0xffa9abb0), frame.getX(), frame.getY(), Colour (0xff1a1b1e), frame.getX(), frame.getBottom(), false));
    g.fillRoundedRectangle (frame, 18.0f);
    g.setGradientFill (ColourGradient (Colour (0xff2c2d31), 0.0f, frame.getY(), Colour (0xff0d0d0f), 0.0f, frame.getBottom(), false));
    g.fillRoundedRectangle (frame.reduced (3.0f), 16.0f);
    g.setColour (Colours::white.withAlpha (0.25f));
    g.drawRoundedRectangle (frame.reduced (3.5f), 16.0f, 0.8f);

    const auto screen = screenArea();
    g.setColour (Colours::black);
    g.fillRoundedRectangle (screen.expanded (2.0f), 12.0f);
    g.setGradientFill (ColourGradient (Colour (0xff1a100a), screen.getCentreX(), screen.getBottom(), Colour (0xff060505), screen.getCentreX(), screen.getY(), false));
    g.fillRoundedRectangle (screen, 10.0f);

    Graphics::ScopedSaveState save (g);
    Path clip;
    clip.addRoundedRectangle (screen, 10.0f);
    g.reduceClipRegion (clip);

    auto area = screen.reduced (18.0f, 12.0f);
    const auto glowText = [&g] (const String& text, Rectangle<float> box, Font f, Justification j, Colour c) {
        g.setFont (f);
        g.setColour (c.withAlpha (0.22f));
        g.drawText (text, box.translated (0.0f, 0.6f).expanded (1.0f), j, false);
        g.setColour (c);
        g.drawText (text, box, j, false);
    };

    // Header line: category (or brand), vibe, tempo + sync link.
    auto top = area.removeFromTop (18.0f);
    glowText (category.isEmpty() ? String ("SHOBEATS FX") : category.toUpperCase(), top, font (Type::title, 18.0f, 0.03f), Justification::centredLeft, amber);
    auto right = top.removeFromRight (190.0f);
    if (hostTempo)
    {
        const auto link = right.removeFromRight (18.0f).withSizeKeepingCentre (15.0f, 10.0f);
        Path chain;
        chain.addRoundedRectangle (link.withWidth (link.getWidth() * 0.6f), 4.0f);
        chain.addRoundedRectangle (link.withTrimmedLeft (link.getWidth() * 0.4f), 4.0f);
        g.setColour (amber);
        g.strokePath (chain, PathStrokeType (1.4f), AffineTransform::rotation (-0.6f, link.getCentreX(), link.getCentreY()));
        right.removeFromRight (4.0f);
    }
    glowText (String (bpm, 1) + " BPM", right, font (Type::labelBold, 18.0f, 0.04f), Justification::centredRight, amber);
    glowText ("VIBE: " + vibe.toUpperCase(), top.withTrimmedLeft (top.getWidth() * 0.45f), font (Type::label, 13.0f, 0.1f), Justification::centredRight,
              amber.withAlpha (0.6f));

    // Preset name and description.
    glowText (name.isEmpty() ? String ("INIT") : name.toUpperCase(), area.removeFromTop (40.0f), font (Type::title, 42.0f, 0.02f),
              Justification::centredLeft, orange.brighter (0.15f));
    g.setFont (font (Type::bodyItalic, 14.0f));
    g.setColour (amber.withAlpha (0.85f));
    g.drawFittedText (description.isEmpty() ? String ("Pick a preset, or choose a vibe and press GENERATE.") : description,
                      area.removeFromTop (20.0f).toNearestInt(), Justification::centredLeft, 1, 0.85f);

    // Live output waveform (real audio, newest on the right).
    const auto w = waveArea();
    {
        const auto base = w.getBottom() - 2.0f;
        g.setColour (orange.withAlpha (0.08f));
        for (int i = 1; i < 4; ++i)
            g.fillRect (w.getX(), w.getY() + w.getHeight() * static_cast<float> (i) / 4.0f, w.getWidth(), 0.6f);
        Path fill, line;
        fill.startNewSubPath (w.getX(), base);
        const auto n = static_cast<int> (wave.size());
        for (int i = 0; i < n; ++i)
        {
            // Perceptual scale: -36 dBFS at the baseline, 0 dBFS at the top.
            const auto db = Decibels::gainToDecibels (wave[static_cast<size_t> (i)], -60.0f);
            const auto v = jlimit (0.0f, 1.0f, (db + 36.0f) / 36.0f);
            const Point<float> p { w.getX() + w.getWidth() * static_cast<float> (i) / static_cast<float> (n - 1), base - v * (w.getHeight() - 4.0f) };
            fill.lineTo (p);
            i == 0 ? line.startNewSubPath (p) : line.lineTo (p);
        }
        fill.lineTo (w.getRight(), base);
        fill.closeSubPath();
        g.setGradientFill (ColourGradient (orange.withAlpha (0.55f), 0.0f, w.getY(), orange.withAlpha (0.04f), 0.0f, base, false));
        g.fillPath (fill);
        for (auto [width, alpha] : { std::pair { 5.0f, 0.08f }, { 3.0f, 0.16f } })
        {
            g.setColour (orange.withAlpha (alpha));
            g.strokePath (line, PathStrokeType (width, PathStrokeType::curved, PathStrokeType::rounded));
        }
        g.setColour (Colour (0xffffc27a));
        g.strokePath (line, PathStrokeType (1.2f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // Status line: chain and global movement.
    auto status = area.removeFromBottom (18.0f);
    auto movement = status.removeFromRight (status.getWidth() * 0.45f);
    glowText ("DRIFT " + String (roundToInt (drift * 100.0f)) + "%   /   UNSTABLE " + String (roundToInt (unstable * 100.0f)) + "%", movement,
              font (Type::label, 13.0f, 0.06f), Justification::centredRight, amber.withAlpha (0.85f));
    StringArray chain;
    for (auto id : order)
        if (active[static_cast<size_t> (id)])
            chain.add (shortName (static_cast<EffectId> (id)));
    const auto anyOn = ! chain.isEmpty();
    hw::drawLed (g, { status.getX() + 4.0f, status.getCentreY() }, 3.0f, amber, true);
    glowText (anyOn ? chain.joinIntoString (" > ") : String ("ALL PEDALS BYPASSED"), status.withTrimmedLeft (14.0f), font (Type::label, 13.0f, 0.06f),
              Justification::centredLeft, amber.withAlpha (0.85f));

    // Glass reflection.
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.07f), screen.getX(), screen.getY(), Colours::white.withAlpha (0.0f), screen.getX(),
                                       screen.getY() + screen.getHeight() * 0.4f, false));
    g.fillRect (screen.withHeight (screen.getHeight() * 0.4f));
}

//==============================================================================
MainBackdrop::MainBackdrop()
{
    setInterceptsMouseClicks (false, false);
    setBufferedToImage (true);
    setOpaque (false);
}

void MainBackdrop::paint (Graphics& g)
{
    const auto f = [] (Rectangle<int> r) { return r.toFloat(); };

    // Upper hardware section.
    hw::drawBezelPanel (g, f (hardwarePlate), 18.0f, 7.0f, true);
    hw::drawLightSeam (g, { f (hardwarePlate).getCentreX(), f (hardwarePlate).getBottom() - 3.5f }, 900.0f, brandOrange, 0.8f);
    for (auto module : { driftModule, unstableModule })
        drawModulePlate (g, f (module));
    // Vent slots under the tubes.
    hw::drawVentSlots (g, f (V (60, 306, 26, 82)), 8);
    hw::drawVentSlots (g, f (V (1174, 306, 26, 82)), 8);
    // Tube housings.
    for (auto t : { driftTubeArea, unstableTubeArea })
        hw::drawTray (g, f (t).expanded (5.0f, 6.0f), 9.0f);

    drawSectionLabel (g, "FX GENERATOR   -   CHOOSE A VIBE", f (V (364, 302, 532, 16)), Justification::centred);
    hw::drawTray (g, f (vibeTray), 12.0f);
    hw::drawSpeakerGrille (g, f (V (330, 416, 132, 48)));
    hw::drawSpeakerGrille (g, f (V (798, 416, 132, 48)));

    // Lower section: FX chain and master.
    hw::drawBezelPanel (g, f (chainPlate), 18.0f, 7.0f, true);
    drawSectionLabel (g, "FX CHAIN   -   CLICK TO SWITCH, DOUBLE-CLICK TO EDIT", f (V (52, 506, 700, 16)));
    drawSectionLabel (g, "MASTER", f (V (1000, 506, 200, 16)));
    hw::drawTray (g, f (chainTray), 14.0f);
    hw::drawTray (g, f (masterTray), 14.0f);
    hw::drawLightSeam (g, { f (masterTray).getCentreX(), f (masterTray).getBottom() - 6.0f }, 170.0f, brandOrange, 0.7f);
    // A machined strip under the master section.
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (f (V (1012, 786, 190, 8)), 4.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (f (V (1012, 786, 190, 8)), 4.0f, 0.8f);
}

//==============================================================================
IndicatorTube::IndicatorTube (AudioProcessorValueTreeState& s, const String& paramId)
{
    setInterceptsMouseClicks (false, false);
    if (auto* p = s.getParameter (paramId))
    {
        attachment = std::make_unique<ParameterAttachment> (*p, [this, p] (float) {
            level = p->getValue();
            repaint();
        });
        attachment->sendInitialUpdate();
    }
}

void IndicatorTube::paint (Graphics& g)
{
    // A faint idle glow keeps the tube readable at 0; the filament brightens with the value.
    hw::drawGlassTube (g, getLocalBounds().toFloat().reduced (2.0f, 0.0f), brandOrange, 0.45f + 0.55f * level);
}

//==============================================================================
MainPanel::MainPanel (ShobeatsAudioProcessor& p, FrameClock& clock)
    : processor (p),
      drift (p.getState(), ParamID::drift, "Drift", brandOrange, Knob::Style::alu),
      unstable (p.getState(), ParamID::unstable, "Unstable", brandOrange, Knob::Style::alu),
      input (p.getState(), ParamID::inputGain, "Input", brandOrange, Knob::Style::panel),
      mix (p.getState(), ParamID::globalMix, "Mix", brandOrange, Knob::Style::panel),
      output (p.getState(), ParamID::outputGain, "Output", brandOrange, Knob::Style::panel),
      driftTube (p.getState(), ParamID::drift),
      unstableTube (p.getState(), ParamID::unstable),
      display (p, clock)
{
    addAndMakeVisible (backdrop);
    for (Component* c : { (Component*) &drift, (Component*) &unstable, (Component*) &input, (Component*) &mix, (Component*) &output,
                          (Component*) &driftTube, (Component*) &unstableTube, (Component*) &display, (Component*) &generate,
                          (Component*) &inputMeter, (Component*) &outputMeter })
        addAndMakeVisible (*c);

    for (auto* k : { &drift, &unstable })
    {
        k->setValueWindow (true);
        k->setCaptionFont (font (Type::labelBold, 25.0f, 0.14f));
    }
    for (auto* k : { &input, &mix, &output })
        k->setCaptionFont (font (Type::labelBold, 15.0f, 0.04f));

    drift.getSlider().setTooltip ("Drift: slow analog wander of pitch, time and rate across all active pedals");
    unstable.getSlider().setTooltip ("Unstable: fast irregular flutter, jitter and level flicker across all active pedals");

    auto& pm = p.getPresetManager();
    const auto names = Generator::vibeNames();
    const KeyCap::Icon icons[] = { KeyCap::Icon::cassette, KeyCap::Icon::cloud, KeyCap::Icon::moon,
                                   KeyCap::Icon::waveform, KeyCap::Icon::planet, KeyCap::Icon::flask };
    for (int i = 0; i < names.size(); ++i)
    {
        auto* key = vibeKeys.add (new KeyCap (names[i], brandOrange));
        key->setIcon (i < 6 ? icons[i] : KeyCap::Icon::none);
        key->setTooltip ("Generator vibe: " + names[i]);
        key->onClick = [this, i] {
            processor.getPresetManager().setSelectedVibe (static_cast<Generator::Vibe> (i));
            timerCallback();
            display.update();
        };
        addAndMakeVisible (key);
    }
    generate.setIcon (KeyCap::Icon::chevrons);
    generate.setTooltip ("Generate a new effect chain in the selected vibe. Like it? Press SAVE.");
    generate.onClick = [&pm] { pm.generate (pm.getSelectedVibe()); };

    for (int i = 0; i < numEffects; ++i)
    {
        auto& tile = tiles[static_cast<size_t> (i)];
        tile = std::make_unique<EffectTile> (p.getState(), static_cast<EffectId> (i), clock, p);
        tile->onOpen = [this] (EffectId id) { if (onOpenEffect) onOpenEffect (id); };
        addAndMakeVisible (*tile);
    }
    shownOrder = p.getChainOrder().get();
    startTimerHz (10);
    timerCallback();
}

void MainPanel::timerCallback()
{
    const auto selected = static_cast<int> (processor.getPresetManager().getSelectedVibe());
    for (int i = 0; i < vibeKeys.size(); ++i)
        vibeKeys[i]->setLit (i == selected);

    const auto order = processor.getChainOrder().get();
    if (order != shownOrder)
    {
        shownOrder = order;
        layoutTiles();
    }
}

void MainPanel::setMeterLevels (float inL, float inR, float outL, float outR)
{
    inputMeter.setLevels (inL, inR);
    outputMeter.setLevels (outL, outR);
}

void MainPanel::paint (Graphics&) {}

void MainPanel::layoutTiles()
{
    const auto tray = chainTray.reduced (18, 12);
    const auto gap = (tray.getWidth() - numEffects * tileW) / (numEffects - 1);
    for (int slot = 0; slot < numEffects; ++slot)
    {
        auto& tile = tiles[static_cast<size_t> (shownOrder[static_cast<size_t> (slot)])];
        tile->setSlot (slot + 1);
        tile->setBounds (tray.getX() + slot * (tileW + gap), tray.getY(), tileW, tileH + 34);
    }
}

void MainPanel::resized()
{
    backdrop.setBounds (getLocalBounds());

    // Big knobs: caption at the top of the module, value window at the bottom.
    for (auto [knob, module] : { std::pair { &drift, driftModule }, { &unstable, unstableModule } })
    {
        knob->setBounds (module.reduced (20, 14));
        knob->getSlider().setTextBoxStyle (Slider::TextBoxBelow, false, 92, 30);
    }
    driftTube.setBounds (driftTubeArea);
    unstableTube.setBounds (unstableTubeArea);
    display.setBounds (displayArea);

    auto vibes = vibeTray.reduced (8, 7);
    const auto w = vibes.getWidth() / jmax (1, vibeKeys.size());
    for (int i = 0; i < vibeKeys.size(); ++i)
        vibeKeys[i]->setBounds (vibes.removeFromLeft (w));
    generate.setBounds (generateArea);

    layoutTiles();

    auto master = masterTray.reduced (8, 12);
    inputMeter.setBounds (master.removeFromLeft (20).withTrimmedBottom (4));
    outputMeter.setBounds (master.removeFromRight (20).withTrimmedBottom (4));
    master.reduce (2, 0);
    const auto kw = master.getWidth() / 3;
    for (auto* k : { &input, &mix, &output })
    {
        k->setBounds (master.removeFromLeft (kw).withSizeKeepingCentre (kw, 140).translated (0, -10));
        k->getSlider().setTextBoxStyle (Slider::TextBoxBelow, false, kw, 24);
    }
}
} // namespace sfx::ui
