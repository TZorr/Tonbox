//
//  PluginProcessor.h
//  Tonbox
//
//  Eight synthesised drum channels on eight MIDI notes, FMBox's three send
//  effects, one stereo output.
//
//  The state is the parameter tree and nothing else: a session or an
//  .tonbox preset restores the kit completely.
//

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/SendEffects.h"
#include "Engine/ToneEngine.h"
#include "ParameterIds.h"

class TonboxProcessor : public juce::AudioProcessor
{
public:
    TonboxProcessor();
    ~TonboxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }   // the longest decay

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Plays one channel from the message thread (a click on its pad). The
        hit lands at the start of the next block. */
    void playChannel (int channel, float velocity);

    const ToneEngine& getEngine() const noexcept { return engine; }

    /** The output's peak per side since the panel last took it - the meter
        beside Master. */
    float takeMeterPeak (int side) noexcept { return meterPeaks[(size_t) (side & 1)].exchange (0.0f); }

    /** Note-ons received so far: the status bar's MIDI light. */
    int getMidiNoteCount() const noexcept { return midiNotes.load (std::memory_order_relaxed); }

    /** The channel's knobs as the engine sees them. */
    ToneEngine::ChannelSettings getChannelSettings (int channel) const;

    int getChannelNote (int channel) const;

    /** The kit's and each channel's name, kept in the state tree (message
        thread). An empty channel name means none was given: the panel then
        describes the sound instead. */
    juce::String getKitName() const;
    juce::String getChannelName (int channel) const;
    void setKitName (const juce::String& name);
    void setChannelName (int channel, const juce::String& name);

    /** Swaps two channels' sounds - every knob, the sends and the name - as
        host gestures. The notes stay where they are: a note belongs to the
        pad's place under the player's fingers, not to the sound (as in
        Kitbox). The target pad becomes the selected one. */
    void swapChannels (int a, int b);

    /** A channel's sound - the same as a swap moves: every knob but the note,
        the sends and the name - as text for the clipboard, in the knobs' own
        units (Hz, ms, dB ...), so it survives a change of knob scales. */
    juce::String copyChannel (int channel) const;

    /** Puts a copied sound onto a channel, as host gestures; the channel's
        note stays. False, and nothing changes, if the text is not a copied
        Tonbox sound. The pasted-on pad becomes the selected one. */
    bool pasteChannel (int channel, const juce::String& text);

    /** Whether the text is a copied Tonbox sound. */
    static bool isCopiedSound (const juce::String& text);

    /** A sound file (.tonboxsound, the copied-sound XML) onto a channel, as a
        paste: the note stays, and what the file leaves out - the sends, the
        choke - stays as it was.
        The channel remembers the file, for the sound browser. */
    juce::Result loadSound (int channel, const juce::File& file);

    /** The channel's sound into a file; the channel takes the file's name. */
    juce::Result saveSound (int channel, const juce::File& file);

    /** The file the channel's sound was last loaded from or saved to, if any. */
    juce::File getChannelSoundFile (int channel) const;

    /** Shift + Level: one channel's Level knob moves every channel's level
        by the same dB, so the kit's balance stays. Measured from the levels
        at the start of the drag, so dragging back restores them exactly, even
        after a channel stopped at -60 or +10 on the way. A channel that is
        off (-60 dB) stays off. Message thread; one gesture per channel. */
    void startLevelGroup (int draggedChannel);
    void moveLevelGroup (float deltaDb);
    void endLevelGroup();
    bool isLevelGroupActive() const noexcept { return levelGroupChannel >= 0; }

    /** The pad last clicked: what the panel edits and Learn assigns. Not saved. */
    int getSelectedChannel() const noexcept       { return selectedChannel.load(); }
    void setSelectedChannel (int channel) noexcept { selectedChannel.store (juce::jlimit (0, TbParams::numChannels - 1, channel)); }

    /** MIDI Learn, as in FMBox: the next note-on becomes this channel's note;
        a channel that had it takes this one's old note. */
    void startLearn (int channel);
    void cancelLearn()                 { learnChannel.store (-1); }
    int getLearnChannel() const noexcept { return learnChannel.load(); }

    /** Message thread (the editor's timer): applies a note the audio thread
        has learnt. */
    void applyLearnedNote();

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout();
    void applyParameters();

    /** Engine, then effects, in pieces no longer than the send buses. */
    void renderSpan (float* left, float* right, int n);

    /** One change, as a host gesture: what a swap or a paste is made of. */
    static void setParameterGesture (juce::RangedAudioParameter& parameter, float normalised);

    /** The suffixes a swap, copy or paste moves: the sound's knobs, the sends. */
    static juce::StringArray soundSuffixes();

    /** The channel's sound as a <TonboxSound> element: copy and save. */
    std::unique_ptr<juce::XmlElement> soundXml (int channel) const;

    using ChannelParams = std::array<std::atomic<float>*, TbParams::Ch::count>;

    struct FxParams
    {
        std::atomic<float>* type = nullptr;
        std::atomic<float>* a = nullptr;
        std::atomic<float>* b = nullptr;
        std::atomic<float>* level = nullptr;
    };

    ToneEngine engine;
    SendEffects effects;
    int preparedBlock = 512;
    std::array<ChannelParams, TbParams::numChannels> channelParams {};
    std::array<std::array<std::atomic<float>*, TbParams::Fx::numSlots>, TbParams::numChannels> sendParams {};
    std::array<FxParams, TbParams::Fx::numSlots> fxParams {};
    std::atomic<float>* masterParam = nullptr;
    std::atomic<int> selectedChannel { 0 }, learnChannel { -1 }, learnedAssignment { -1 };   // (channel << 8) | note
    std::array<std::atomic<float>, TbParams::numChannels> pendingHits;
    std::array<float, TbParams::numChannels> levelGroupStart {};
    std::array<std::atomic<float>, 2> meterPeaks {};
    std::atomic<int> midiNotes { 0 };
    int levelGroupChannel = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TonboxProcessor)
};
