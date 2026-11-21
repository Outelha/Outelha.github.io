#include "PedalLive.h"

namespace sfx::ui
{
namespace
{
float normalised (const juce::AudioProcessorValueTreeState& s, const char* id)
{
    if (auto* p = s.getParameter (id))
        return p->getValue();
    return 0.0f;
}

float raw (const juce::AudioProcessorValueTreeState& s, const char* id)
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
    return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (peak, -60.0f) + 48.0f) / 48.0f);
}
} // namespace

PedalLive::PedalLive (const juce::AudioProcessorValueTreeState& s, EffectId id, const VisualFeed& f) : parameters (s), effect (id), feed (f)
{
    readParameters (parameters, effect, shown);
    lastSamples = feed.samplesProcessed();
}

void PedalLive::readParameters (const juce::AudioProcessorValueTreeState& s, EffectId id, pedal::State& out)
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

bool PedalLive::advance (double dt, bool& animating)
{
    pedal::State next = shown;
    readParameters (parameters, effect, next);

    // Audio is running while the processor keeps consuming samples (stops when the host
    // stops processing or bypasses the plugin).
    const auto samples = feed.samplesProcessed();
    const bool running = samples != lastSamples;
    lastSamples = samples;

    bool moving = false;
    const auto target = next.enabled && running ? feedLevel (feed) : 0.0f;
    level += (target - level) * static_cast<float> (juce::jmin (1.0, dt * (target > level ? 20.0 : 6.0)));
    if (level < 0.002f)
        level = 0.0f;
    next.level = level;
    if (effect == EffectId::tape || effect == EffectId::resampler)
        moving = ! juce::exactlyEqual (level, shown.level);

    if (effect == EffectId::tape)
    {
        // Reels spin up while tape runs and coast to a stop when it stops or is bypassed.
        const auto targetSpeed = next.enabled && running ? 2.4f : 0.0f;
        reelSpeed += (targetSpeed - reelSpeed) * static_cast<float> (juce::jmin (1.0, dt * 3.0));
        if (reelSpeed < 0.01f && targetSpeed <= 0.0f)
            reelSpeed = 0.0f;
        reelPhase = std::fmod (reelPhase + reelSpeed * static_cast<float> (dt), juce::MathConstants<float>::twoPi * 100.0f);
        moving = moving || reelSpeed > 0.0f;
    }
    next.reelPhase = reelPhase;

    if ((effect == EffectId::chorus || effect == EffectId::tremolo || effect == EffectId::delay) && next.enabled)
    {
        animTime += dt;
        moving = true;
    }
    next.time = animTime;

    const bool changed = moving || next.enabled != shown.enabled || ! juce::exactlyEqual (next.knobA, shown.knobA)
                         || ! juce::exactlyEqual (next.knobB, shown.knobB) || next.extra != shown.extra;
    // Keep interaction flags owned by the component.
    next.selected = shown.selected;
    next.hover = shown.hover;
    shown = next;
    animating = animating || moving;
    return changed;
}
} // namespace sfx::ui
