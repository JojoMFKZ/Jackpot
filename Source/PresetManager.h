#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

class JackpotProcessor;

namespace jackpot
{
// Saves / loads patches as small XML files in
//   macOS:   ~/Documents/JACKPOT/Presets
//   Windows: Documents\JACKPOT\Presets
// plus a handful of factory patches compiled into the plugin.
class PresetManager
{
public:
    explicit PresetManager (JackpotProcessor& p) : proc (p) {}

    static juce::File folder();
    static constexpr const char* extension = ".jackpot";

    juce::Array<juce::File> userPresets() const;
    juce::StringArray factoryNames() const;

    bool save (const juce::String& name);
    bool loadFile (const juce::File& f);
    void loadFactory (int index);
    bool deleteFile (const juce::File& f);

    // steps through factory + user presets in one list
    void step (int direction);

private:
    JackpotProcessor& proc;
    int position = -1;
};
} // namespace jackpot
