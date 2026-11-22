//
//  SoundLibrary.cpp
//  Tonbox
//

#include "SoundLibrary.h"
#include "ParameterIds.h"
#include "PresetButton.h"

namespace
{
    juce::Array<juce::File> sortedChildren (const juce::File& folder, int whatToFind, const juce::String& pattern)
    {
        auto children = folder.findChildFiles (whatToFind, false, pattern);
        std::sort (children.begin(), children.end(), [] (const juce::File& a, const juce::File& b)
        {
            return a.getFileName().compareNatural (b.getFileName()) < 0;
        });
        return children;
    }
}

namespace SoundLibrary
{
    juce::File folder()
    {
        const auto sounds = PresetButton::folder().getChildFile ("Sounds");
        sounds.createDirectory();
        return sounds;
    }

    juce::Array<juce::File> groups (const juce::File& root)
    {
        const juce::StringArray order { "BD", "SD", "HH", "PERC", "FX" };
        auto found = sortedChildren (root, juce::File::findDirectories, "*");

        std::stable_sort (found.begin(), found.end(), [&] (const juce::File& a, const juce::File& b)
        {
            const auto rank = [&] (const juce::File& f)
            {
                const auto i = order.indexOf (f.getFileName(), true);
                return i >= 0 ? i : order.size();
            };
            return rank (a) < rank (b);
        });

        return found;
    }

    juce::Array<juce::File> sounds (const juce::File& group)
    {
        return sortedChildren (group, juce::File::findFiles, juce::String ("*") + TbParams::soundExtension);
    }

    juce::File step (const juce::File& group, const juce::File& current, int delta)
    {
        const auto list = sounds (group);

        if (list.isEmpty())
            return {};

        const auto index = list.indexOf (current);

        if (index < 0)
            return delta >= 0 ? list.getFirst() : list.getLast();

        const auto n = list.size();
        return list[((index + delta) % n + n) % n];
    }

    juce::String listPrefix (const juce::String& soundName)
    {
        const auto word = soundName.trim().upToFirstOccurrenceOf (" ", false, false);
        const auto stem = word.upToFirstOccurrenceOf ("-", false, false);
        return stem.isNotEmpty() ? stem : word;
    }

    juce::File groupOf (const juce::File& root, const juce::File& sound)
    {
        const auto parent = sound.getParentDirectory();
        return parent.getParentDirectory() == root ? parent : juce::File();
    }
}
