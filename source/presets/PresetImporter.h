#pragma once

#include "Preset.h"

namespace sfx
{
// Converts third-party effect presets into native SHOBEATS presets.
//
// Supported sources (XML):
//  - TONEFLX  (<SonixAudio product="TONEFLX">, .fwd)
//  - FFWD FX  (<FFWD product="FFWD FX">, .fwd) incl. parameters stored without a value
//  - single-module snippets (<Core0_<Module> ...attributes...>, .fwd / .fmod)
//
// The conversion maps each source module onto the closest SHOBEATS pedal. It is an
// approximation: units and algorithms differ. Every approximation and every source
// feature that SHOBEATS cannot reproduce is listed in the result's notes, and a preset
// is marked "partial" when a source feature that was actually in use has no equivalent.
// See docs/IMPORT.md for the full mapping table and the evidence behind it.
class PresetImporter
{
public:
    enum class Status
    {
        converted, // every active module mapped; only approximations
        partial,   // some active source features have no SHOBEATS equivalent
        failed     // not a supported preset file
    };

    struct Result
    {
        Status status = Status::failed;
        Preset preset;
        juce::String sourcePath; // relative path in the archive / folder
        juce::String error;      // for failed
    };

    // Converts one preset file's text. 'relativePath' gives folder (category) and file name.
    static Result convert (const juce::String& xmlText, const juce::String& relativePath);

    // Folder path -> display category, e.g. "2. CREATIVE FX/ANALOG WARMTH" -> "Creative FX / Analog Warmth".
    static juce::String categoryFromFolder (const juce::String& folderPath);

    // "! @someproducer - WARM KEYS II" -> creator "@someproducer", name "WARM KEYS II".
    static void splitCreator (const juce::String& fileStem, juce::String& creator, juce::String& name);

    static bool isSupportedFile (const juce::File& f);

    struct BatchReport
    {
        int converted = 0, partial = 0, failed = 0, duplicates = 0;
        juce::StringArray lines; // human-readable per-file report
        juce::File reportFile;
        int imported() const noexcept { return converted + partial; }
    };

    // Imports a .zip, a folder, or a single preset file into 'importRoot' as native preset
    // files (kept in the archive's folder structure). Files whose content was already
    // imported before are skipped. Writes an import report next to the presets.
    static BatchReport importInto (const juce::File& source, const juce::File& importRoot);
};

juce::String toString (PresetImporter::Status);
} // namespace sfx
