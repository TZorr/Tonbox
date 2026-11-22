//
//  EditorShot.cpp
//  Tonbox (from FMBox)
//
//  Builds the plugin headlessly, checks what needs the whole processor, and
//  paints the panel into a PNG.
//
//  - the session round trip: every parameter through getStateInformation
//    into a fresh processor, with the knobs moved immediately before saving -
//    which is when a state saved from the tree instead of copyState() comes
//    back stale;
//  - every channel plays from its note, on the sample the MIDI event names;
//  - a click on a pad plays that channel; Learn takes a note and swaps;
//  - names: the kit's and each channel's travel with the state, a state from
//    before names leaves them empty, and swapping two channels swaps sounds,
//    sends and names but leaves the notes where they are;
//  - presets: the preset menu's .tonbox saves and loads knobs and kit name,
//    another plugin's preset and an .aupreset are refused, and the converted library loads;
//  - sounds: a pad's sound saves to a .tonboxsound and loads onto another pad
//    (note kept, sends kept when the file has none), the library lists its
//    groups and steps round, the file a sound came from travels with the
//    state and a swap, a paste forgets it; the display's browser steps;
//  - Shift + Level: every level by the same dB from the drag's start, off stays off;
//  - the knobs' own scales: Pitch 632 Hz and Level -12 dB at the middle,
//    Attack 110 ms, and every scale maps back to the position it came from.
//
//  Look at the PNG after every layout change.
//
//  Usage:  EditorShot <output-directory>
//          EditorShot --check-sounds <sounds-folder>   loads every sound in it and
//                                                      checks every value arrived
//

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginEditor.h"
#include "Engine/PresetFile.h"
#include "UI/Knob.h"
#include "UI/Palette.h"
#include "PluginProcessor.h"
#include "UI/SoundLibrary.h"

#include <cstdio>

namespace
{
    int failures = 0, checks = 0;

    void check (bool condition, const juce::String& what, const juce::String& detail = {})
    {
        ++checks;

        if (! condition)
        {
            ++failures;
            std::printf ("FAIL: %s  %s\n", what.toRawUTF8(), detail.toRawUTF8());
        }
    }

    void setParam (TonboxProcessor& processor, const juce::String& id, float value)
    {
        auto* parameter = processor.parameters.getParameter (id);
        jassert (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    float getParam (TonboxProcessor& processor, const juce::String& id)
    {
        return processor.parameters.getRawParameterValue (id)->load();
    }

    bool writePng (const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        juce::PNGImageFormat png;
        return stream != nullptr && png.writeImageToStream (image, *stream);
    }

    void pump (int ms)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
    }

    using namespace TbParams;

    //==============================================================================
    void checkStateRoundTrip()
    {
        TonboxProcessor original;
        original.prepareToPlay (48000.0, 512);

        setParam (original, channelId (1, Ch::pitch), 260.0f);
        setParam (original, channelId (2, Ch::wave), 2.0f);
        setParam (original, channelId (3, Ch::pan), -40.0f);
        setParam (original, channelId (0, Ch::level), 0.6f);
        setParam (original, channelId (4, Ch::filterQ), 25.0f);
        setParam (original, channelId (5, Ch::modAmount), -12.0f);
        setParam (original, channelId (6, Ch::stereo), 1.0f);
        setParam (original, channelId (7, Ch::note), 60.0f);
        setParam (original, master, 0.7f);
        setParam (original, channelId (2, Ch::sends[Fx::delay]), 0.45f);
        setParam (original, Fx::ids[Fx::reverb].type, 2.0f);
        setParam (original, Fx::ids[Fx::mod].b, 0.25f);
        setParam (original, Fx::ids[Fx::delay].level, -9.0f);

        juce::MemoryBlock state;
        original.getStateInformation (state);

        TonboxProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());

        int compared = 0, differing = 0;

        for (auto* parameter : original.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            {
                ++compared;
                const auto a = getParam (original, withId->paramID);
                const auto b = getParam (restored, withId->paramID);

                if (std::abs (a - b) > 1.0e-3f * std::max (1.0f, std::abs (a)))
                {
                    ++differing;
                    std::printf ("  %s: %f -> %f\n", withId->paramID.toRawUTF8(), a, b);
                }
            }

        check (compared == numChannels * (Ch::count + Fx::numSlots) + Fx::numSlots * 4 + 1, "every parameter is in the state", juce::String (compared));
        check (differing == 0, "the session round trip restores every parameter", juce::String (differing) + " differ");
    }

    void checkMappings()
    {
        TonboxProcessor processor;

        const auto at = [&] (const char* suffix, float normalised)
        {
            auto* parameter = processor.parameters.getParameter (channelId (0, suffix));
            return parameter->convertFrom0to1 (normalised);
        };

        const auto near = [] (float a, float b, float tolerance) { return std::abs (a - b) <= tolerance * std::abs (b); };

        // Tonbox's own scales (Engine/Curves.h).
        check (near (at (Ch::pitch, 0.5f), 632.46f, 0.002f), "Pitch at 0.5 is 632 Hz (logarithmic)", juce::String (at (Ch::pitch, 0.5f)));
        check (near (at (Ch::oscDecay, 0.5f), 316.23f, 0.002f), "Decay at 0.5 is 316 ms (logarithmic)", juce::String (at (Ch::oscDecay, 0.5f)));
        check (near (at (Ch::oscAttack, 0.5f), 109.98f, 0.003f), "Attack at 0.5 is 110 ms", juce::String (at (Ch::oscAttack, 0.5f)));
        check (near (at (Ch::filterQ, 0.5f), 31.62f, 0.002f), "Q at 0.5 is 31.6 (logarithmic)", juce::String (at (Ch::filterQ, 0.5f)));
        check (near (at (Ch::modAmount, 0.75f), 12.0f, 0.002f), "Mod Amount at 0.75 is +12 st", juce::String (at (Ch::modAmount, 0.75f)));
        check (near (at (Ch::level, 0.5f), -12.0f, 0.002f), "Level at the middle is -12 dB", juce::String (at (Ch::level, 0.5f)));
        check (near (Curves::driveGain (30.0f), 1.0f, 0.01f) && near (Curves::driveGain (100.0f), 250.0f, 0.01f),
               "Drive 30 is a drive of 1, 100 is 250");
        check (std::isinf (Curves::modDecayMs (0.0f)) && near (Curves::modDecayMs (1.0f), 2.0f, 0.001f),
               "Mod Rate in Decay mode runs from hold to 2 ms");

        // And back: a stored value lands on the position it came from.
        auto* pitch = processor.parameters.getParameter (channelId (0, Ch::pitch));
        check (std::abs (pitch->convertTo0to1 (632.46f) - 0.5f) < 1.0e-4f, "Pitch maps back to its position");
        auto* attack = processor.parameters.getParameter (channelId (0, Ch::oscAttack));
        check (std::abs (attack->convertTo0to1 (attack->convertFrom0to1 (0.37f)) - 0.37f) < 1.0e-4f, "Attack maps back to its position");
        auto* amount = processor.parameters.getParameter (channelId (0, Ch::modAmount));
        check (std::abs (amount->convertTo0to1 (amount->convertFrom0to1 (0.81f)) - 0.81f) < 1.0e-4f, "Mod Amount maps back to its position");
        check (std::abs (Curves::driveToAmount (Curves::driveGain (42.0f)) - 42.0f) < 0.01f, "Drive maps back to its position");
        check (std::abs (Curves::modDecayToX (Curves::modDecayMs (0.3f)) - 0.3f) < 1.0e-4f
                   && std::abs (Curves::modSineToX (Curves::modSineHz (0.6f)) - 0.6f) < 1.0e-4f
                   && std::abs (Curves::modNoiseToX (Curves::modNoiseHz (0.7f)) - 0.7f) < 1.0e-4f,
               "Mod Rate maps back to its position in every mode");
    }

    /** First sample in the block above -100 dB, or -1. */
    int firstSound (const juce::AudioBuffer<float>& buffer)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (std::abs (buffer.getSample (0, i)) > 1.0e-5f || std::abs (buffer.getSample (1, i)) > 1.0e-5f)
                return i;
        return -1;
    }

    void checkMidi()
    {
        for (int c = 0; c < numChannels; ++c)
        {
            TonboxProcessor processor;
            processor.prepareToPlay (48000.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (10, kit[c].note, (juce::uint8) 127), 300);
            processor.processBlock (buffer, midi);

            const auto first = firstSound (buffer);
            const auto name = "Channel " + juce::String (c + 1);
            check (processor.getEngine().getHitCount (c) == 1, name + " plays from note " + juce::String (kit[c].note));
            check (first >= 300 && first <= 303, name + " starts on the sample the note is on", juce::String (first));
        }

        TonboxProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);
        processor.processBlock (buffer, midi);
        check (firstSound (buffer) < 0, "a note outside the kit plays nothing");

        processor.playChannel (6, 0.8f);
        midi.clear();
        processor.processBlock (buffer, midi);
        check (processor.getEngine().getHitCount (6) == 1 && firstSound (buffer) == 0,
               "a pad click plays its channel at the start of the next block");
    }

    void checkNames()
    {
        {
            TonboxProcessor fresh;
            check (fresh.getKitName() == defaultKitName && fresh.getChannelName (0) == "Kick",
                   "a new instance has the default kit's names");
        }

        TonboxProcessor original;
        original.setKitName ("Test Kit & <Co>");
        original.setChannelName (3, "Hat \"Closed\"");
        juce::MemoryBlock state;
        original.getStateInformation (state);

        TonboxProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.getKitName() == "Test Kit & <Co>" && restored.getChannelName (3) == "Hat \"Closed\""
                   && restored.getChannelName (0) == "Kick",
               "names travel with the state", restored.getKitName() + " / " + restored.getChannelName (3));

        // A state saved before 0.3.0 has no names.
        {
            auto xml = original.parameters.copyState().createXml();
            xml->removeAttribute (kitNameId);
            for (int c = 0; c < numChannels; ++c)
                xml->removeAttribute (channelNameId (c));
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary (*xml, old);
            TonboxProcessor fromOld;
            fromOld.setStateInformation (old.getData(), (int) old.getSize());
            check (fromOld.getKitName().isEmpty() && fromOld.getChannelName (2).isEmpty(), "an old state brings no names");
        }

        // Swap channels 1 and 4 (index 0 and 3).
        TonboxProcessor processor;
        processor.setChannelName (3, "Hat");
        setParam (processor, channelId (0, Ch::sends[Fx::reverb]), 0.6f);
        const auto pitch0 = getParam (processor, channelId (0, Ch::pitch)), pitch3 = getParam (processor, channelId (3, Ch::pitch));
        const auto wave0 = getParam (processor, channelId (0, Ch::wave)), wave3 = getParam (processor, channelId (3, Ch::wave));
        const auto note0 = processor.getChannelNote (0), note3 = processor.getChannelNote (3);

        processor.swapChannels (0, 3);

        check (std::abs (getParam (processor, channelId (0, Ch::pitch)) - pitch3) < 0.01f
                   && std::abs (getParam (processor, channelId (3, Ch::pitch)) - pitch0) < 0.01f
                   && juce::exactlyEqual (getParam (processor, channelId (0, Ch::wave)), wave3)
                   && juce::exactlyEqual (getParam (processor, channelId (3, Ch::wave)), wave0),
               "a swap moves the sounds");
        check (std::abs (getParam (processor, channelId (3, Ch::sends[Fx::reverb])) - 0.6f) < 1.0e-4f
                   && getParam (processor, channelId (0, Ch::sends[Fx::reverb])) < 0.2f,
               "a swap moves the sends");
        check (processor.getChannelName (0) == "Hat" && processor.getChannelName (3) == "Kick", "a swap moves the names");
        check (processor.getChannelNote (0) == note0 && processor.getChannelNote (3) == note3, "a swap leaves the notes in place");
        check (processor.getSelectedChannel() == 3, "the target pad is selected after a swap");

        processor.swapChannels (2, 2);
        check (processor.getChannelName (2) == kit[2].role, "a swap with itself changes nothing");
    }

    void checkCopyPaste()
    {
        TonboxProcessor processor;
        setParam (processor, channelId (0, Ch::sends[Fx::delay]), 0.35f);
        setParam (processor, channelId (0, Ch::modAmount), 17.5f);
        const auto note4 = processor.getChannelNote (4);
        const auto text = processor.copyChannel (0);

        check (TonboxProcessor::isCopiedSound (text), "a copied sound is recognised");
        check (! TonboxProcessor::isCopiedSound ("hello") && ! processor.pasteChannel (4, "<Other/>"),
               "other text is not pasted");
        check (processor.pasteChannel (4, text), "a copied sound pastes");

        int differing = 0;
        for (const auto* suffix : Ch::all)
            if (juce::String (suffix) != Ch::note
                && std::abs (getParam (processor, channelId (0, suffix)) - getParam (processor, channelId (4, suffix)))
                       > 1.0e-4f * std::max (1.0f, std::abs (getParam (processor, channelId (0, suffix)))))
                ++differing;
        for (const auto* suffix : Ch::sends)
            if (std::abs (getParam (processor, channelId (0, suffix)) - getParam (processor, channelId (4, suffix))) > 1.0e-5f)
                ++differing;

        check (differing == 0, "paste makes every knob and send the copy's", juce::String (differing) + " differ");
        check (processor.getChannelName (4) == processor.getChannelName (0), "paste brings the name");
        check (processor.getChannelNote (4) == note4, "paste leaves the note in place");
        check (processor.getSelectedChannel() == 4, "the pasted-on pad is selected");
        check (processor.copyChannel (4) == text, "a pasted sound copies back to the same text");
    }

    /** One block with one note-on at sample 0. */
    void playNote (TonboxProcessor& processor, int note)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        processor.processBlock (buffer, midi);
    }

    void checkLearn()
    {
        TonboxProcessor processor;
        processor.prepareToPlay (48000.0, 512);

        processor.setSelectedChannel (5);
        processor.startLearn (processor.getSelectedChannel());
        check (processor.getLearnChannel() == 5, "Learn waits for the selected pad");
        playNote (processor, 60);
        processor.applyLearnedNote();
        check (processor.getLearnChannel() < 0, "Learn stops after one note");
        check (processor.getChannelNote (5) == 60, "channel 6 learns note 60", juce::String (processor.getChannelNote (5)));
        check (processor.getEngine().getHitCount (5) == 0, "the learning note still plays only what it played before");

        processor.startLearn (0);
        playNote (processor, 37);
        processor.applyLearnedNote();
        check (processor.getChannelNote (0) == 37 && processor.getChannelNote (1) == 36, "learning channel 2's note swaps 1 and 2",
               juce::String (processor.getChannelNote (0)) + " / " + juce::String (processor.getChannelNote (1)));

        processor.startLearn (6);
        processor.cancelLearn();
        playNote (processor, 72);
        processor.applyLearnedNote();
        check (processor.getChannelNote (6) == kit[6].note, "a cancelled Learn changes nothing");
    }
}

namespace
{
    void checkPresets()
    {
        const auto folder = juce::File::createTempFile ("tonbox-presets");
        folder.createDirectory();
        const auto file = folder.getChildFile (juce::String ("Test Kit") + PresetFile::extension);

        TonboxProcessor original;
        setParam (original, channelId (2, Ch::pitch), 321.0f);
        original.setKitName ("Test Kit");
        check (PresetButton::save (original, file).wasOk(), "a preset saves");

        const auto text = file.loadFileAsString();
        const auto xml = juce::XmlDocument::parse (text);
        check (juce::String (PresetFile::extension) == ".tonbox" && xml != nullptr && xml->hasTagName ("TZorrPreset")
                   && xml->getStringAttribute ("plugin") == "Tnbx" && xml->getStringAttribute ("name") == "Test Kit"
                   && xml->getChildByName ("Tonbox") != nullptr,
               "the preset is readable XML naming Tnbx", text.substring (0, 120));

        TonboxProcessor loaded;
        check (PresetButton::load (loaded, file).wasOk(), "a preset loads");
        check (std::abs (getParam (loaded, channelId (2, Ch::pitch)) - 321.0f) < 0.5f && loaded.getKitName() == "Test Kit",
               "a preset brings back the knobs and the kit name", juce::String (getParam (loaded, channelId (2, Ch::pitch))));

        // Another plugin's preset: refused, and nothing changes.
        const auto foreign = folder.getChildFile (juce::String ("Foreign") + PresetFile::extension);
        foreign.replaceWithText (text.replace ("plugin=\"Tnbx\"", "plugin=\"Fmbx\""));
        TonboxProcessor untouched;
        check (PresetButton::load (untouched, foreign).failed() && untouched.getKitName() == defaultKitName,
               "another plugin's preset is refused");

        // An Audio Unit preset is Logic's, not ours.
        const auto auPreset = folder.getChildFile ("Logic.aupreset");
        auPreset.replaceWithText ("<?xml version=\"1.0\"?><plist version=\"1.0\"><dict/></plist>");
        check (PresetButton::load (untouched, auPreset).failed() && untouched.getKitName() == defaultKitName,
               "an .aupreset is refused");

        // The library in the preset folder, if there is one: its first preset loads.
        const auto library = PresetButton::folder().findChildFiles (juce::File::findFiles, true,
                                                                    juce::String ("*") + PresetFile::extension);

        if (! library.isEmpty())
        {
            TonboxProcessor fromLibrary;
            check (PresetButton::load (fromLibrary, library.getFirst()).wasOk() && fromLibrary.getKitName().isNotEmpty(),
                   "a preset from the library loads", library.getFirst().getFullPathName());
        }

        folder.deleteRecursively();
    }
}

namespace
{
    juce::File makeSound (const juce::File& folder, const juce::String& name, float pitch)
    {
        // As the converter writes it: the sound's knobs, no sends, no choke.
        juce::XmlElement xml ("TonboxSound");
        xml.setAttribute ("version", 1);
        xml.setAttribute ("name", name);
        for (const auto& [id, value] : { std::pair<const char*, double> { Ch::pitch, pitch }, { Ch::wave, 2.0 },
                                         { Ch::level, -6.5 }, { Ch::filterQ, 3.25 } })
        {
            auto* v = xml.createNewChildElement ("V");
            v->setAttribute ("id", id);
            v->setAttribute ("value", value);
        }
        folder.createDirectory();
        const auto file = folder.getChildFile (name + soundExtension);
        file.replaceWithText (xml.toString());
        return file;
    }

    void checkSounds (const juce::File& root)
    {
        const auto bd = root.getChildFile ("BD"), sd = root.getChildFile ("SD");
        const auto bd1 = makeSound (bd, "T BD 1", 50.0f), bd2 = makeSound (bd, "T BD 2", 60.0f), bd10 = makeSound (bd, "T BD 10", 70.0f);
        const auto sd1 = makeSound (sd, "T SD Snap", 210.0f);
        root.getChildFile ("MY").createDirectory();
        root.getChildFile ("FX").createDirectory();

        const auto groups = SoundLibrary::groups (root);
        juce::StringArray names;
        for (const auto& g : groups)
            names.add (g.getFileName());
        check (names.joinIntoString (" ") == "BD SD FX MY", "the groups: BD SD HH PERC FX first, then the rest", names.joinIntoString (" "));

        const auto list = SoundLibrary::sounds (bd);
        check (list.size() == 3 && list[0] == bd1 && list[1] == bd2 && list[2] == bd10, "a group lists in natural order");
        check (SoundLibrary::step (bd, bd10, 1) == bd1 && SoundLibrary::step (bd, bd1, -1) == bd10
                   && SoundLibrary::step (bd, bd1, 1) == bd2, "stepping wraps round");
        check (SoundLibrary::step (bd, sd1, 1) == bd1 && SoundLibrary::step (bd, {}, -1) == bd10,
               "stepping from outside the group starts at its first or last");
        check (SoundLibrary::listPrefix ("KT BD Deep") == "KT" && SoundLibrary::listPrefix ("XR-1 BD") == "XR"
                   && SoundLibrary::listPrefix ("XR-ABC SD 2") == "XR" && SoundLibrary::listPrefix ("Kick") == "Kick"
                   && SoundLibrary::listPrefix ("-X Y") == "-X",
               "the list's submenus: the first word, up to a hyphen");
        check (SoundLibrary::groupOf (root, bd2) == bd && SoundLibrary::groupOf (root, root.getChildFile ("x" + juce::String (soundExtension))) == juce::File(),
               "a sound's group is its folder in the library");

        // Load: the file's knobs, the note and sends stay.
        TonboxProcessor processor;
        setParam (processor, channelId (5, Ch::sends[Fx::delay]), 0.45f);
        setParam (processor, channelId (5, Ch::choke), 1.0f);
        const auto note5 = processor.getChannelNote (5);
        check (processor.loadSound (5, bd2).wasOk(), "a sound loads");
        check (std::abs (getParam (processor, channelId (5, Ch::pitch)) - 60.0f) < 0.01f
                   && juce::exactlyEqual (getParam (processor, channelId (5, Ch::wave)), 2.0f)
                   && std::abs (getParam (processor, channelId (5, Ch::level)) + 6.5f) < 0.01f
                   && std::abs (getParam (processor, channelId (5, Ch::filterQ)) - 3.25f) < 0.01f,
               "a loaded sound sets its knobs", juce::String (getParam (processor, channelId (5, Ch::pitch))));
        check (processor.getChannelNote (5) == note5, "a loaded sound leaves the note");
        check (std::abs (getParam (processor, channelId (5, Ch::sends[Fx::delay])) - 0.45f) < 1.0e-4f
                   && getParam (processor, channelId (5, Ch::choke)) > 0.5f,
               "what the file leaves out - sends, choke - stays");
        check (processor.getChannelName (5) == "T BD 2" && processor.getChannelSoundFile (5) == bd2,
               "the pad takes the sound's name and remembers its file");

        const auto bogus = root.getChildFile ("bogus" + juce::String (soundExtension));
        bogus.replaceWithText ("<Other/>");
        check (processor.loadSound (5, bogus).failed() && processor.getChannelSoundFile (5) == bd2
                   && processor.loadSound (5, root.getChildFile ("missing.tonboxsound")).failed(),
               "a file that is not a sound is refused, and nothing changes");

        // Save and load back: every knob and send the same.
        setParam (processor, channelId (5, Ch::modAmount), -12.25f);
        const auto saved = sd.getChildFile ("Mine" + juce::String (soundExtension));
        check (processor.saveSound (5, saved).wasOk() && processor.getChannelName (5) == "Mine"
                   && processor.getChannelSoundFile (5) == saved, "a sound saves, and the pad takes the file's name");

        TonboxProcessor other;
        check (other.loadSound (2, saved).wasOk(), "a saved sound loads");
        auto copy = other.copyChannel (2);
        check (copy == processor.copyChannel (5), "a saved sound comes back the same", copy.substring (0, 100));

        // The file travels with the state and a swap; a paste and a kit forget it.
        juce::MemoryBlock state;
        other.getStateInformation (state);
        TonboxProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        check (restored.getChannelSoundFile (2) == saved, "the sound's file travels with the state");

        restored.swapChannels (2, 7);
        check (restored.getChannelSoundFile (7) == saved && restored.getChannelSoundFile (2) == juce::File(),
               "a swap moves the sound's file");

        restored.pasteChannel (7, processor.copyChannel (0));
        check (restored.getChannelSoundFile (7) == juce::File(), "a pasted sound comes from no file");

        TonboxProcessor fresh;
        juce::MemoryBlock kitState;
        fresh.getStateInformation (kitState);
        other.setStateInformation (kitState.getData(), (int) kitState.getSize());
        check (other.getChannelSoundFile (2) == juce::File(), "a kit brings no sound files of its own");
    }

    void checkLevelGroup()
    {
        TonboxProcessor processor;
        const float start[] = { -6.0f, -12.0f, -60.0f, 8.0f, -20.0f, -1.0f, 0.0f, -30.0f };
        for (int c = 0; c < numChannels; ++c)
            setParam (processor, channelId (c, Ch::level), start[c]);

        const auto level = [&] (int c) { return getParam (processor, channelId (c, Ch::level)); };
        const auto near = [] (float a, float b) { return std::abs (a - b) < 0.02f; };

        processor.startLevelGroup (0);
        check (processor.isLevelGroupActive(), "Shift + Level starts a group drag");

        setParam (processor, channelId (0, Ch::level), -9.0f);   // the dragged knob, as the slider sets it
        processor.moveLevelGroup (-3.0f);
        check (near (level (1), -15.0f) && near (level (3), 5.0f) && near (level (7), -33.0f) && near (level (0), -9.0f),
               "every level moves by the same dB", juce::String (level (1)) + " " + juce::String (level (3)));
        check (juce::exactlyEqual (level (2), -60.0f), "a channel that is off stays off");

        processor.moveLevelGroup (5.0f);
        check (near (level (3), 10.0f) && near (level (1), -7.0f), "a level stops at +10 dB", juce::String (level (3)));

        processor.moveLevelGroup (-3.0f);
        check (near (level (3), 5.0f) && near (level (6), -3.0f),
               "dragging back restores the balance from the start of the drag", juce::String (level (3)));

        processor.endLevelGroup();
        processor.moveLevelGroup (-10.0f);
        check (! processor.isLevelGroupActive() && near (level (3), 5.0f), "after the drag only the knob itself moves");
    }

    /** Every sound in the folder through loadSound, every value compared with
        the file's - clamped to the knob's range, as loading does. */
    int checkSoundLibrary (const juce::File& folder)
    {
        const auto files = folder.findChildFiles (juce::File::findFiles, true, juce::String ("*") + soundExtension);
        int bad = 0, values = 0, clamped = 0;

        for (const auto& file : files)
        {
            TonboxProcessor processor;
            const auto xml = juce::parseXML (file);

            if (xml == nullptr || processor.loadSound (0, file).failed()
                || processor.getChannelName (0) != xml->getStringAttribute ("name"))
            {
                std::printf ("FAIL: %s does not load\n", file.getFullPathName().toRawUTF8());
                ++bad;
                continue;
            }

            for (auto* v : xml->getChildWithTagNameIterator ("V"))
            {
                const auto id = channelId (0, v->getStringAttribute ("id").toRawUTF8());
                auto* parameter = processor.parameters.getParameter (id);
                const auto wanted = (float) v->getDoubleAttribute ("value");

                if (parameter == nullptr)
                {
                    std::printf ("FAIL: %s: unknown id %s\n", file.getFileName().toRawUTF8(), id.toRawUTF8());
                    ++bad;
                    continue;
                }

                const auto& range = parameter->getNormalisableRange();
                const auto expected = juce::jlimit (range.start, range.end, wanted);
                clamped += juce::exactlyEqual (expected, wanted) ? 0 : 1;
                const auto got = getParam (processor, id);
                ++values;

                if (std::abs (got - expected) > 1.0e-4f * std::max (1.0f, std::abs (expected)))
                {
                    std::printf ("FAIL: %s: %s is %g, the file says %g\n", file.getFileName().toRawUTF8(),
                                 id.toRawUTF8(), (double) got, (double) expected);
                    ++bad;
                }
            }
        }

        std::printf ("%d sounds, %d values checked, %d clamped to the knob's range, %d failure(s)\n",
                     files.size(), values, clamped, bad);
        return bad == 0 && ! files.isEmpty() ? 0 : 1;
    }
}

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc > 2 && juce::String (argv[1]) == "--check-sounds")
        return checkSoundLibrary (juce::File (juce::String (argv[2])));

    const juce::File outputDirectory = argc > 1 ? juce::File (juce::String (argv[1]))
                                                : juce::File::getCurrentWorkingDirectory();

    if (! outputDirectory.createDirectory())
    {
        std::printf ("FAIL: cannot create %s\n", outputDirectory.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("Session round trip...\n");
    checkStateRoundTrip();
    std::printf ("Knob mappings...\n");
    checkMappings();
    std::printf ("MIDI notes and pads...\n");
    checkMidi();
    std::printf ("Learn...\n");
    checkLearn();
    std::printf ("Names and swapping...\n");
    checkNames();
    std::printf ("Copy and paste...\n");
    checkCopyPaste();
    std::printf ("Presets...\n");
    checkPresets();
    std::printf ("Shift + Level...\n");
    checkLevelGroup();
    std::printf ("Sounds...\n");
    const auto soundRoot = juce::File::createTempFile ("tonbox-sounds");
    checkSounds (soundRoot);

    // The panel, with a few pads mid-flash and channel 2 selected.
    {
        TonboxProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        processor.setSelectedChannel (1);
        setParam (processor, Fx::ids[Fx::reverb].type, 1.0f);   // Room

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        pump (50);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127), 0);
        midi.addEvent (juce::MidiMessage::noteOn (10, 39, (juce::uint8) 80), 10);
        processor.processBlock (buffer, midi);

        pump (80);

        // The display follows the pad focus, not the notes: two notes on
        // channels 1 and 4 came in, channel 2 is still the one shown.
        auto* tonboxEditor = dynamic_cast<TonboxEditor*> (editor.get());
        check (tonboxEditor != nullptr, "the editor is Tonbox's");
        auto& panel = tonboxEditor->getPanel();

        if (auto* display = panel.findChildWithID ("display"))
            check (dynamic_cast<Display*> (display)->getShownChannel() == 1,
                   "the display stays on the selected pad while notes come in",
                   juce::String (dynamic_cast<Display*> (display)->getShownChannel()));

        // Shift + Level on the panel: the knob's drag moves the others too.
        if (auto* knob = dynamic_cast<Knob*> (panel.findChildWithID ("level")))
        {
            const auto before = getParam (processor, channelId (4, Ch::level));
            juce::ModifierKeys::currentModifiers = juce::ModifierKeys (juce::ModifierKeys::shiftModifier);
            knob->onDragStart();
            knob->setValue (knob->getValue() - 4.0, juce::sendNotificationSync);
            knob->onDragEnd();
            juce::ModifierKeys::currentModifiers = juce::ModifierKeys();
            check (std::abs (getParam (processor, channelId (4, Ch::level)) - (before - 4.0f)) < 0.05f && ! processor.isLevelGroupActive(),
                   "a Shift drag on the panel's Level moves the other channels", juce::String (getParam (processor, channelId (4, Ch::level))));

            const auto after = getParam (processor, channelId (4, Ch::level));
            knob->onDragStart();
            knob->setValue (knob->getValue() + 4.0, juce::sendNotificationSync);
            knob->onDragEnd();
            check (juce::exactlyEqual (getParam (processor, channelId (4, Ch::level)), after), "without Shift only the knob's channel moves");
        }
        else
            check (false, "the Level knob is on the panel");

        // The display's browser, on the test library.
        if (auto* display = dynamic_cast<Display*> (panel.findChildWithID ("display")))
            {
                display->setLibraryRoot (soundRoot);
                check (display->getGroups().size() == 4 && display->getSoundIndex() < 0,
                       "the browser lists the library's groups", juce::String (display->getGroups().size()));

                display->selectGroup (0);
                display->stepSound (1);
                check (processor.getChannelSoundFile (1).getFileName() == "T BD 1" + juce::String (soundExtension)
                           && display->getSoundIndex() == 0 && display->getGroupSize() == 3,
                       "> loads the group's first sound onto the selected pad", processor.getChannelSoundFile (1).getFileName());

                display->stepSound (-1);
                check (processor.getChannelName (1) == "T BD 10" && display->getSoundIndex() == 2, "< wraps round to the last");

                display->selectGroup (1);
                check (display->getBrowseGroup() == 1 && display->getSoundIndex() < 0
                           && processor.getChannelName (1) == "T BD 10", "a group button loads nothing");

                processor.setSelectedChannel (0);
                display->refresh();
                check (display->getBrowseGroup() == 1, "a pad with no sound file keeps the group browsed");

                processor.setSelectedChannel (1);
                display->refresh();
                check (display->getBrowseGroup() == 0 && display->getSoundIndex() == 2,
                       "a pad's sound from the library shows its group and place");

                display->selectGroup (0);
                display->stepSound (1);
                display->stepSound (1);   // T BD 2, for the picture
                pump (80);
            }

        {
            // The knobs, name over dial: each in the window, none on another.
            std::vector<Knob*> knobs;
            for (auto* child : panel.getChildren())
                if (auto* knob = dynamic_cast<Knob*> (child))
                    knobs.push_back (knob);

            int outside = 0, overlapping = 0;
            for (size_t i = 0; i < knobs.size(); ++i)
            {
                const auto bounds = knobs[i]->getBounds();
                outside += ! panel.getLocalBounds().contains (bounds);
                for (size_t j = i + 1; j < knobs.size(); ++j)
                    overlapping += bounds.intersects (knobs[j]->getBounds());
            }

            check (knobs.size() > 20, "the panel has its knobs", juce::String ((int) knobs.size()));
            check (outside == 0, "every knob sits in the window", juce::String (outside));
            check (overlapping == 0, "and none lies on another", juce::String (overlapping));

            // The host moves knobs, as automation would: the names stay.
            setParam (processor, channelId (1, Ch::pitch), 300.0f);
            setParam (processor, master, 0.5f);
            pump (60);

            int showingValue = 0;
            for (auto* knob : knobs)
                showingValue += knob->isShowingValue();

            check (showingValue == 0, "a change from the host leaves every knob showing its name", juce::String (showingValue));
        }

        Palette::setDark (false);
        editor->repaint();
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        const auto file = outputDirectory.getChildFile ("tonbox.png");
        check (writePng (image, file), "the panel renders to " + file.getFullPathName());

        Palette::setDark (true);
        editor->repaint();
        check (writePng (editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f),
                         outputDirectory.getChildFile ("tonbox-dark.png")), "the panel renders dark");
        Palette::setDark (false);
        editor->repaint();
        std::printf ("Panel: %d x %d\n", editor->getWidth(), editor->getHeight());

        processor.editorBeingDeleted (editor.get());
        editor.reset();
    }

    soundRoot.deleteRecursively();

    std::printf ("\n%d checks, %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
