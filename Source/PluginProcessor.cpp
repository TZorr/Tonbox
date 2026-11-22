//
//  PluginProcessor.cpp
//  Tonbox
//

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ValueText.h"

namespace
{
    /** A range whose 0..1 position follows one of the measured curves. */
    juce::NormalisableRange<float> curveRange (float start, float end, float (*from) (float), float (*to) (float))
    {
        return { start, end,
                 [from] (float, float, float x) { return from (x); },
                 [to]   (float, float, float v) { return juce::jlimit (0.0f, 1.0f, to (v)); },
                 [] (float lo, float hi, float v) { return juce::jlimit (lo, hi, v); } };
    }

    float modAmountFrom (float x) { return Curves::modSemitones (2.0f * x - 1.0f); }
    float modAmountTo (float st)  { return 0.5f * (Curves::modSemitonesToU (st) + 1.0f); }
    float attackFrom (float x)    { return Curves::attackMs (x); }
    float attackTo (float ms)     { return Curves::attackToX (ms); }

    /** Level and Master in dB, -60 (off) .. +10, with the middle of the knob
        at -12 dB as on FMBox. */
    juce::NormalisableRange<float> levelRange()
    {
        juce::NormalisableRange<float> range (Curves::minLevelDb, Curves::maxLevelDb);
        range.setSkewForCentre (-12.0f);
        return range;
    }

    /** A default Mod Rate given in its mode's unit (ms or Hz), as a knob position. */
    float modRateKnob (int mode, float value)
    {
        switch (mode)
        {
            case ToneEngine::modSine:  return Curves::modSineToX (value);
            case ToneEngine::modNoise: return Curves::modNoiseToX (value);
            default:                   return Curves::modDecayToX (value);
        }
    }
}

TonboxProcessor::TonboxProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "Tonbox", makeParameterLayout())
{
    using namespace TbParams;

    for (int c = 0; c < numChannels; ++c)
    {
        for (int k = 0; k < Ch::count; ++k)
            channelParams[(size_t) c][(size_t) k] = parameters.getRawParameterValue (channelId (c, Ch::all[k]));

        for (int f = 0; f < Fx::numSlots; ++f)
            sendParams[(size_t) c][(size_t) f] = parameters.getRawParameterValue (channelId (c, Ch::sends[f]));

        pendingHits[(size_t) c].store (0.0f);
    }

    for (int f = 0; f < Fx::numSlots; ++f)
    {
        const auto& ids = Fx::ids[f];
        fxParams[(size_t) f] = { parameters.getRawParameterValue (ids.type), parameters.getRawParameterValue (ids.a),
                                 parameters.getRawParameterValue (ids.b), parameters.getRawParameterValue (ids.level) };
    }

    masterParam = parameters.getRawParameterValue (master);

    // A new instance's names: the default kit's. A restored state brings its
    // own - or none, if it was saved before 0.3.0.
    parameters.state.setProperty (kitNameId, defaultKitName, nullptr);
    for (int c = 0; c < numChannels; ++c)
        parameters.state.setProperty (channelNameId (c), kit[c].role, nullptr);
}

juce::AudioProcessorValueTreeState::ParameterLayout TonboxProcessor::makeParameterLayout()
{
    using namespace TbParams;
    using Attributes = juce::AudioParameterFloatAttributes;
    using TextFunction = std::function<juce::String (float, int)>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto frequencyRange = curveRange (20.0f, 20000.0f, Curves::frequency, Curves::frequencyToX);
    const auto attackRange    = curveRange (0.0f, 10000.0f, attackFrom, attackTo);
    const auto decayRange     = curveRange (10.0f, 10000.0f, Curves::decayMs, Curves::decayToX);
    const auto qRange         = curveRange (0.1f, 10000.0f, Curves::filterQ, Curves::filterQToX);
    const auto amountRange    = curveRange (-96.0f, 96.0f, modAmountFrom, modAmountTo);

    const TextFunction hzText      = [] (float v, int) { return ValueText::hz (v); };
    const TextFunction msText      = [] (float v, int) { return ValueText::ms (v); };
    const TextFunction stText      = [] (float v, int) { return ValueText::semitones (v); };
    const TextFunction qText       = [] (float v, int) { return ValueText::q (v); };
    const TextFunction levelText   = [] (float v, int) { return ValueText::level (v); };
    const TextFunction dbText      = [] (float v, int) { return ValueText::db (v); };
    const TextFunction panText     = [] (float v, int) { return ValueText::pan (v); };
    const TextFunction plainText   = [] (float v, int) { return juce::String (juce::roundToInt (v)); };
    const TextFunction percentText = [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; };
    const TextFunction unitText    = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };

    for (int c = 0; c < numChannels; ++c)
    {
        const auto& k = kit[c];
        const auto name = "Ch " + juce::String (c + 1);
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("c" + juce::String (c + 1), name, " | ");

        const auto add = [&] (const char* suffix, const juce::String& label, juce::NormalisableRange<float> range,
                              float value, const TextFunction& text)
        {
            group->addChild (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { channelId (c, suffix), stateVersion }, name + " " + label, range, value,
                Attributes().withStringFromValueFunction (text)));
        };

        const auto choice = [&] (const char* suffix, const juce::String& label, const juce::StringArray& names, int value)
        {
            group->addChild (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { channelId (c, suffix), stateVersion }, name + " " + label, names, value));
        };

        const auto toggle = [&] (const char* suffix, const juce::String& label, bool value)
        {
            group->addChild (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { channelId (c, suffix), stateVersion }, name + " " + label, value));
        };

        choice (Ch::wave,       "Osc Wave",     waveNames, k.wave);
        add    (Ch::pitch,      "Osc Pitch",    frequencyRange, k.pitchHz, hzText);
        add    (Ch::oscAttack,  "Osc Attack",   attackRange, k.oscAttackMs, msText);
        add    (Ch::oscDecay,   "Osc Decay",    decayRange, k.oscDecayMs, msText);
        choice (Ch::modMode,    "Mod Mode",     modModeNames, k.modMode);
        add    (Ch::modRate,    "Mod Rate",     { 0.0f, 1.0f }, modRateKnob (k.modMode, k.modRate), unitText);
        add    (Ch::modAmount,  "Mod Amount",   amountRange, k.modAmountSt, stText);
        choice (Ch::filterMode, "Noise Filter", filterNames, k.filterMode);
        add    (Ch::filterFreq, "Noise Freq",   frequencyRange, k.filterHz, hzText);
        add    (Ch::filterQ,    "Noise Q",      qRange, k.filterQ, qText);
        toggle (Ch::stereo,     "Noise Stereo", k.stereo);
        choice (Ch::envMode,    "Noise Env",    envModeNames, k.envMode);
        add    (Ch::noiseAttack, "Noise Attack", attackRange, k.noiseAttackMs, msText);
        add    (Ch::noiseDecay, "Noise Decay",  decayRange, k.noiseDecayMs, msText);
        add    (Ch::mix,        "Osc/Noise Mix", { 0.0f, 100.0f }, k.mix, plainText);
        add    (Ch::distortion, "Drive",        { 0.0f, 100.0f }, k.drive, plainText);
        add    (Ch::eqFreq,     "EQ Freq",      frequencyRange, k.eqHz, hzText);
        add    (Ch::eqGain,     "EQ Gain",      { -40.0f, 40.0f }, k.eqGainDb, dbText);
        add    (Ch::level,      "Level",        levelRange(), k.levelDb, levelText);
        add    (Ch::pan,        "Pan",          { -100.0f, 100.0f }, k.pan, panText);
        toggle (Ch::choke,      "Choke",        k.choke);
        add    (Ch::oscVel,     "Osc Velocity",   { 0.0f, 200.0f }, k.oscVel, percentText);
        add    (Ch::noiseVel,   "Noise Velocity", { 0.0f, 200.0f }, k.noiseVel, percentText);
        add    (Ch::modVel,     "Mod Velocity",   { 0.0f, 200.0f }, k.modVel, percentText);

        group->addChild (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { channelId (c, Ch::note), stateVersion }, name + " Note", 0, 127, k.note,
            juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return noteName (v) + " " + juce::String (v); })));

        // 0.2.0 (version hint 2): the sends into the three effects.
        const float sendDefaults[] = { k.reverb, k.delay, k.mod };

        for (int f = 0; f < Fx::numSlots; ++f)
            group->addChild (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { channelId (c, Ch::sends[f]), 2 }, name + " " + Fx::titles[f] + " Send",
                juce::NormalisableRange<float> (0.0f, 1.0f), sendDefaults[f], Attributes().withStringFromValueFunction (unitText)));

        layout.add (std::move (group));
    }

    // The send effects: Type, the effect's two knobs, the return level. A and
    // B mean whatever the loaded effect calls them (EffectCatalog), so here
    // they are plain 0..1 - the panel prints them in the effect's own units.
    {
        juce::NormalisableRange<float> returnRange (Fx::minLevelDb, Fx::maxLevelDb);
        returnRange.setSkewForCentre (-12.0f);
        const TextFunction returnText = [] (float v, int)
        {
            return v <= Fx::minLevelDb + 0.01f ? juce::String ("-inf") : ValueText::db (v);
        };

        auto fxGroup = std::make_unique<juce::AudioProcessorParameterGroup> ("fx", "Effects", " | ");

        for (int f = 0; f < Fx::numSlots; ++f)
        {
            const auto& ids = Fx::ids[f];
            const juce::String title (Fx::titles[f]);
            const auto category = Fx::categories[f];

            juce::StringArray types;
            for (int t = 0; t < EffectCatalog::numSubtypes (category); ++t)
                types.add (EffectCatalog::info (category, t).name);

            fxGroup->addChild (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { ids.type, 2 }, title + " Type", types, Fx::defaultTypes[f]));
            fxGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { ids.a, 2 }, title + " A", juce::NormalisableRange<float> (0.0f, 1.0f), Fx::defaultA[f],
                Attributes().withStringFromValueFunction (unitText)));
            fxGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { ids.b, 2 }, title + " B", juce::NormalisableRange<float> (0.0f, 1.0f), Fx::defaultB[f],
                Attributes().withStringFromValueFunction (unitText)));
            fxGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { ids.level, 2 }, title + " Level", returnRange, 0.0f,
                Attributes().withStringFromValueFunction (returnText)));
        }

        layout.add (std::move (fxGroup));
    }

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { master, stateVersion }, "Master", levelRange(),
        0.0f, Attributes().withStringFromValueFunction (levelText)));

    return layout;
}

//==============================================================================
void TonboxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    applyParameters();
    preparedBlock = juce::jmax (1, samplesPerBlock);
    engine.prepare (sampleRate, preparedBlock);
    effects.prepare (sampleRate, preparedBlock);
}

bool TonboxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

ToneEngine::ChannelSettings TonboxProcessor::getChannelSettings (int channel) const
{
    using namespace TbParams::Ch;
    const auto& p = channelParams[(size_t) channel];
    const auto get = [&p] (int index) { return p[(size_t) index]->load(); };
    const auto pick = [&get] (int index) { return (int) std::lround (get (index)); };

    ToneEngine::ChannelSettings s;
    s.wave          = pick (iWave);
    s.pitchHz       = get (iPitch);
    s.oscAttackMs   = get (iOscAttack);
    s.oscDecayMs    = get (iOscDecay);
    s.modMode       = pick (iModMode);
    s.modRate       = get (iModRate);
    s.modAmount     = get (iModAmount);
    s.filterMode    = pick (iFilterMode);
    s.filterHz      = get (iFilterFreq);
    s.filterQ       = get (iFilterQ);
    s.stereo        = get (iStereo) > 0.5f;
    s.envMode       = pick (iEnvMode);
    s.noiseAttackMs = get (iNoiseAttack);
    s.noiseDecayMs  = get (iNoiseDecay);
    s.mix           = get (iMix);
    s.distortion    = get (iDistortion);
    s.eqHz          = get (iEqFreq);
    s.eqGainDb      = get (iEqGain);
    s.levelDb       = get (iLevel);
    s.pan           = get (iPan);
    s.choke         = get (iChoke) > 0.5f;
    s.oscVelocity   = get (iOscVel) * 0.01f;
    s.noiseVelocity = get (iNoiseVel) * 0.01f;
    s.modVelocity   = get (iModVel) * 0.01f;

    for (size_t f = 0; f < s.sends.size(); ++f)
        s.sends[f] = sendParams[(size_t) channel][f]->load();

    return s;
}

void TonboxProcessor::applyParameters()
{
    for (int c = 0; c < TbParams::numChannels; ++c)
        engine.setChannel (c, getChannelSettings (c));

    engine.setMaster (masterParam->load());

    for (int f = 0; f < TbParams::Fx::numSlots; ++f)
    {
        const auto& p = fxParams[(size_t) f];
        effects.setSlot (f, { (int) std::lround (p.type->load()), p.a->load(), p.b->load(),
                              TbParams::Fx::levelToGain (p.level->load()) });
    }
}

void TonboxProcessor::renderSpan (float* left, float* right, int n)
{
    while (n > 0)
    {
        const auto piece = juce::jmin (n, preparedBlock);
        engine.render (left, right, piece);
        effects.process (engine, left, right, piece);
        left += piece;
        right += piece;
        n -= piece;
    }
}

void TonboxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
    {
        buffer.clear();
        return;
    }

    applyParameters();

    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto bpm = position->getBpm())
                effects.setTempo (*bpm);

    for (int c = 0; c < TbParams::numChannels; ++c)
        if (const auto velocity = pendingHits[(size_t) c].exchange (0.0f); velocity > 0.0f)
            engine.trigger (c, velocity);

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const auto numSamples = buffer.getNumSamples();

    std::array<int, TbParams::numChannels> notes {};
    for (int c = 0; c < TbParams::numChannels; ++c)
        notes[(size_t) c] = getChannelNote (c);

    // Rendered in pieces between MIDI events, so every hit starts on the
    // sample the host put it on.
    int rendered = 0;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (! message.isNoteOn())
            continue;

        const auto position = juce::jlimit (rendered, numSamples, metadata.samplePosition);

        if (position > rendered)
        {
            renderSpan (left + rendered, right + rendered, position - rendered);
            rendered = position;
        }

        const auto note = message.getNoteNumber();
        midiNotes.fetch_add (1, std::memory_order_relaxed);

        // Learn takes the note for the channel that asked, and the note still
        // plays whatever it played - hearing the key is the confirmation.
        if (const auto learning = learnChannel.exchange (-1); learning >= 0)
            learnedAssignment.store ((learning << 8) | note);

        for (int c = 0; c < TbParams::numChannels; ++c)
            if (notes[(size_t) c] == note)
                engine.trigger (c, message.getFloatVelocity());
    }

    if (numSamples > rendered)
        renderSpan (left + rendered, right + rendered, numSamples - rendered);

    // The meter beside Master: the peak, held until the panel takes it.
    for (int side = 0; side < 2; ++side)
    {
        const auto range = juce::FloatVectorOperations::findMinAndMax (side == 0 ? left : right, numSamples);
        const auto magnitude = juce::jmax (-range.getStart(), range.getEnd());
        auto& held = meterPeaks[(size_t) side];
        auto current = held.load (std::memory_order_relaxed);
        while (magnitude > current && ! held.compare_exchange_weak (current, magnitude)) {}
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

int TonboxProcessor::getChannelNote (int channel) const
{
    return (int) std::lround (channelParams[(size_t) channel][TbParams::Ch::iNote]->load());
}

void TonboxProcessor::startLearn (int channel)
{
    learnedAssignment.store (-1);
    learnChannel.store (juce::jlimit (0, TbParams::numChannels - 1, channel));
}

void TonboxProcessor::applyLearnedNote()
{
    const auto learned = learnedAssignment.exchange (-1);

    if (learned < 0)
        return;

    const auto channel = learned >> 8, note = learned & 0x7f;
    const auto previous = getChannelNote (channel);

    const auto setNote = [this] (int c, int n)
    {
        auto* parameter = parameters.getParameter (TbParams::channelId (c, TbParams::Ch::note));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) n));
        parameter->endChangeGesture();
    };

    // Whoever had the note takes this channel's old one: every channel stays
    // playable and no note plays two.
    for (int c = 0; c < TbParams::numChannels; ++c)
        if (c != channel && getChannelNote (c) == note)
            setNote (c, previous);

    setNote (channel, note);
}

juce::String TonboxProcessor::getKitName() const
{
    return parameters.state.getProperty (TbParams::kitNameId).toString();
}

juce::String TonboxProcessor::getChannelName (int channel) const
{
    return parameters.state.getProperty (TbParams::channelNameId (channel)).toString();
}

void TonboxProcessor::setKitName (const juce::String& name)
{
    parameters.state.setProperty (TbParams::kitNameId, name.trim(), nullptr);
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void TonboxProcessor::setChannelName (int channel, const juce::String& name)
{
    if (channel < 0 || channel >= TbParams::numChannels)
        return;

    parameters.state.setProperty (TbParams::channelNameId (channel), name.trim(), nullptr);
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

void TonboxProcessor::setParameterGesture (juce::RangedAudioParameter& parameter, float normalised)
{
    parameter.beginChangeGesture();
    parameter.setValueNotifyingHost (normalised);
    parameter.endChangeGesture();
}

juce::StringArray TonboxProcessor::soundSuffixes()
{
    using namespace TbParams;
    juce::StringArray suffixes;

    for (const auto* suffix : Ch::all)
        if (juce::String (suffix) != Ch::note)
            suffixes.add (suffix);

    for (const auto* suffix : Ch::sends)
        suffixes.add (suffix);

    return suffixes;
}

void TonboxProcessor::swapChannels (int a, int b)
{
    using namespace TbParams;

    if (a == b || a < 0 || b < 0 || a >= numChannels || b >= numChannels)
        return;

    for (const auto& suffix : soundSuffixes())
    {
        auto* pa = parameters.getParameter (channelId (a, suffix.toRawUTF8()));
        auto* pb = parameters.getParameter (channelId (b, suffix.toRawUTF8()));
        const auto va = pa->getValue(), vb = pb->getValue();
        setParameterGesture (*pa, vb);
        setParameterGesture (*pb, va);
    }

    // The name and the file the sound came from go with it.
    for (const auto& [idA, idB] : { std::pair { channelNameId (a), channelNameId (b) },
                                    std::pair { channelSoundId (a), channelSoundId (b) } })
    {
        const auto va = parameters.state.getProperty (idA), vb = parameters.state.getProperty (idB);
        parameters.state.setProperty (idA, vb, nullptr);
        parameters.state.setProperty (idB, va, nullptr);
    }

    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));

    setSelectedChannel (b);
}

namespace
{
    constexpr const char* copiedSoundTag = "TonboxSound";
}

std::unique_ptr<juce::XmlElement> TonboxProcessor::soundXml (int channel) const
{
    using namespace TbParams;

    auto sound = std::make_unique<juce::XmlElement> (copiedSoundTag);
    sound->setAttribute ("version", 1);
    sound->setAttribute ("name", getChannelName (channel));

    for (const auto& suffix : soundSuffixes())
    {
        auto* parameter = parameters.getParameter (channelId (channel, suffix.toRawUTF8()));
        auto* value = sound->createNewChildElement ("V");
        value->setAttribute ("id", suffix);
        value->setAttribute ("value", parameter->convertFrom0to1 (parameter->getValue()));
    }

    return sound;
}

juce::String TonboxProcessor::copyChannel (int channel) const
{
    return soundXml (channel)->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader());
}

bool TonboxProcessor::isCopiedSound (const juce::String& text)
{
    if (! text.trimStart().startsWith ("<" + juce::String (copiedSoundTag)))
        return false;

    const auto xml = juce::parseXML (text);
    return xml != nullptr && xml->hasTagName (copiedSoundTag) && xml->getIntAttribute ("version") == 1;
}

bool TonboxProcessor::pasteChannel (int channel, const juce::String& text)
{
    using namespace TbParams;

    if (channel < 0 || channel >= numChannels || ! isCopiedSound (text))
        return false;

    const auto xml = juce::parseXML (text);
    const auto allowed = soundSuffixes();

    for (auto* value : xml->getChildWithTagNameIterator ("V"))
    {
        const auto suffix = value->getStringAttribute ("id");

        // Only the sound's own knobs: never the note, never another channel.
        if (! allowed.contains (suffix) || ! value->hasAttribute ("value"))
            continue;

        if (auto* parameter = parameters.getParameter (channelId (channel, suffix.toRawUTF8())))
            setParameterGesture (*parameter, parameter->convertTo0to1 ((float) value->getDoubleAttribute ("value")));
    }

    parameters.state.setProperty (channelNameId (channel), xml->getStringAttribute ("name"), nullptr);
    parameters.state.removeProperty (channelSoundId (channel), nullptr);   // from no file
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));

    setSelectedChannel (channel);
    return true;
}

juce::Result TonboxProcessor::loadSound (int channel, const juce::File& file)
{
    using namespace TbParams;

    if (channel < 0 || channel >= numChannels)
        return juce::Result::fail ("No such channel.");

    if (! file.existsAsFile())
        return juce::Result::fail (file.getFullPathName() + " does not exist.");

    // Through the parser, so a file with an XML declaration or edited by hand
    // reads as well as the converter's.
    const auto xml = juce::parseXML (file);
    const auto text = xml != nullptr ? xml->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader())
                                     : juce::String();

    if (! pasteChannel (channel, text))
        return juce::Result::fail (file.getFileName() + " is not a Tonbox sound.");

    if (getChannelName (channel).isEmpty())
        parameters.state.setProperty (channelNameId (channel), file.getFileNameWithoutExtension(), nullptr);

    parameters.state.setProperty (channelSoundId (channel), file.getFullPathName(), nullptr);
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    return juce::Result::ok();
}

juce::Result TonboxProcessor::saveSound (int channel, const juce::File& file)
{
    using namespace TbParams;

    if (channel < 0 || channel >= numChannels)
        return juce::Result::fail ("No such channel.");

    parameters.state.setProperty (channelNameId (channel), file.getFileNameWithoutExtension(), nullptr);

    if (! file.replaceWithText (soundXml (channel)->toString (juce::XmlElement::TextFormat().withoutHeader())))
        return juce::Result::fail ("Could not write " + file.getFullPathName() + ".");

    parameters.state.setProperty (channelSoundId (channel), file.getFullPathName(), nullptr);
    updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
    return juce::Result::ok();
}

juce::File TonboxProcessor::getChannelSoundFile (int channel) const
{
    const auto path = parameters.state.getProperty (TbParams::channelSoundId (channel)).toString();
    return juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
}

void TonboxProcessor::startLevelGroup (int draggedChannel)
{
    using namespace TbParams;

    if (isLevelGroupActive())
        endLevelGroup();

    levelGroupChannel = juce::jlimit (0, numChannels - 1, draggedChannel);

    for (int c = 0; c < numChannels; ++c)
    {
        levelGroupStart[(size_t) c] = parameters.getRawParameterValue (channelId (c, Ch::level))->load();

        // The dragged knob's own gesture is the slider's.
        if (c != levelGroupChannel)
            parameters.getParameter (channelId (c, Ch::level))->beginChangeGesture();
    }
}

void TonboxProcessor::moveLevelGroup (float deltaDb)
{
    using namespace TbParams;

    if (! isLevelGroupActive())
        return;

    for (int c = 0; c < numChannels; ++c)
    {
        const auto start = levelGroupStart[(size_t) c];

        if (c == levelGroupChannel || start <= Curves::minLevelDb)
            continue;   // the dragged one moves itself; off stays off

        auto* parameter = parameters.getParameter (channelId (c, Ch::level));
        const auto db = juce::jlimit (Curves::minLevelDb, Curves::maxLevelDb, start + deltaDb);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (db));
    }
}

void TonboxProcessor::endLevelGroup()
{
    using namespace TbParams;

    if (! isLevelGroupActive())
        return;

    for (int c = 0; c < numChannels; ++c)
        if (c != levelGroupChannel)
            parameters.getParameter (channelId (c, Ch::level))->endChangeGesture();

    levelGroupChannel = -1;
}

void TonboxProcessor::playChannel (int channel, float velocity)
{
    if (channel >= 0 && channel < TbParams::numChannels)
        pendingHits[(size_t) channel].store (juce::jlimit (0.01f, 1.0f, velocity));
}

//==============================================================================
//  copyState(), never the tree directly: a knob's live value reaches the tree
//  only when the APVTS flushes, and copyState() flushes first.
void TonboxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TonboxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* TonboxProcessor::createEditor()
{
    return new TonboxEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TonboxProcessor();
}
