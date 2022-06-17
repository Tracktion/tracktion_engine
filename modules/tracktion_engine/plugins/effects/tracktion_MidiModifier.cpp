/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

MidiModifierPlugin::MidiModifierPlugin (PluginCreationInfo info) : Plugin (info)
{
    semitones = addParam ("semitones up", TRANS("Semitones"),
                          { -getMaximumSemitones(), getMaximumSemitones() },
                          [] (float value)            { return std::abs (value) < 0.01f ? "(" + TRANS("Original pitch") + ")"
                                                                                        : getSemitonesAsString (value); },
                          [] (const juce::String& s)  { return juce::jlimit (-MidiModifierPlugin::getMaximumSemitones(),
                                                                             MidiModifierPlugin::getMaximumSemitones(),
                                                                             s.getFloatValue()); });

    semitonesValue.referTo (state, IDs::semitonesUp, getUndoManager());
    semitones->attachToCurrentValue (semitonesValue);
}

MidiModifierPlugin::~MidiModifierPlugin()
{
    notifyListenersOfDeletion();

    semitones->detachFromCurrentValue();
}

const char* MidiModifierPlugin::xmlTypeName ("midiModifier");

juce::String MidiModifierPlugin::getName()                                         { return TRANS("MIDI Modifier"); }
juce::String MidiModifierPlugin::getPluginType()                                   { return xmlTypeName; }
juce::String MidiModifierPlugin::getShortName (int)                                { return TRANS("MIDI Modifier"); }
void MidiModifierPlugin::initialise (const PluginInitialisationInfo&)              {}
void MidiModifierPlugin::deinitialise()                                            {}
double MidiModifierPlugin::getLatencySeconds()                                     { return 0.0; }
int MidiModifierPlugin::getNumOutputChannelsGivenInputs (int)                      { return 0; }
void MidiModifierPlugin::getChannelNames (juce::StringArray*, juce::StringArray*)  {}
bool MidiModifierPlugin::takesAudioInput()                                         { return false; }
bool MidiModifierPlugin::canBeAddedToClip()                                        { return false; }
bool MidiModifierPlugin::needsConstantBufferSize()                                 { return false; }

void MidiModifierPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    if (fc.bufferForMidiMessages != nullptr)
        fc.bufferForMidiMessages->addToNoteNumbers (juce::roundToInt (semitones->getCurrentValue()));
}

juce::String MidiModifierPlugin::getSelectableDescription()
{
    return TRANS("MIDI Modifier Plugin");
}

void MidiModifierPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    juce::CachedValue<float>* cvsFloat[]  = { &semitonesValue, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

}} // namespace tracktion { inline namespace engine
