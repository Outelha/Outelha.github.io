#pragma once

#include "ChainOrder.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace sfx
{
// One preset as stored on disk / in the factory library.
//
// JSON shape:
// {
//   "name": "...", "description": "...", "category": "...",   (category optional per preset)
//   "chain": ["tape", "sat", "reverb"],   active pedals in processing order; all others are switched off
//   "order": [ ...8 keys... ],            optional full order (user presets); otherwise chain + the rest
//   "params": { "tapeWow": 0.35, "satMode": "Warm", "delaySync": true, ... }   real units / choice names
// }
// Parameters not listed are reset to their defaults when the preset is applied.
struct Preset
{
    juce::String name, category, description;
    bool isFactory = true;
    juce::File file; // user and imported presets

    // Imported presets only (converted from another plugin's preset files).
    bool isImported = false;
    juce::String creator;          // original author, when the source names one
    juce::String sourceProduct;    // e.g. "TONEFLX 1.5.1"
    juce::String sourceFile;       // path inside the imported archive / folder
    juce::String sourceHash;       // fingerprint of the source file, used to skip duplicates
    juce::String conversionStatus; // "converted" or "partial"
    juce::StringArray conversionNotes;

    std::vector<int> chain;      // effect ids, processing order of the active pedals
    ChainOrder::Order order {};  // full order to apply
    juce::NamedValueSet params;  // id -> value (number, bool or choice name)

    static std::optional<Preset> fromVar (const juce::var& v, const juce::String& defaultCategory, juce::String* error = nullptr);
    juce::var toVar() const;

    // Captures the processor's current settings as a preset.
    static Preset capture (juce::AudioProcessorValueTreeState& state, const ChainOrder& order);

    // Writes the preset into the parameters (notifying the host) and the chain order.
    void applyTo (juce::AudioProcessorValueTreeState& state, ChainOrder& chainOrder) const;
};

int effectIdForKey (const juce::String& key);
} // namespace sfx
