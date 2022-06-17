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

StepClip::Channel::Channel (StepClip& c, const juce::ValueTree& v)
    : clip (c), state (v)
{
    auto* um = &clip.edit.getUndoManager();

    channel.referTo (state, IDs::channel, um, MidiChannel (defaultMidiChannel));
    noteNumber.referTo (state, IDs::note, um, defaultNoteNumber);
    noteValue.referTo (state, IDs::velocity, um, defaultNoteValue);
    grooveTemplate.referTo (state, IDs::groove, um);
    grooveStrength.referTo (state, IDs::grooveStrength, um, 0.1f);
    name.referTo (state, IDs::name, um);
}

StepClip::Channel::~Channel() noexcept
{
    notifyListenersOfDeletion();
}

bool StepClip::Channel::operator== (const Channel& other) const noexcept
{
    return channel == other.channel
            && noteNumber == other.noteNumber
            && noteValue == other.noteValue
            && grooveTemplate == other.grooveTemplate
            && name == other.name
            && grooveStrength == other.grooveStrength;
}

bool StepClip::Channel::usesGrooveStrength() const
{
    auto gt = clip.edit.engine.getGrooveTemplateManager().getTemplateByName (grooveTemplate.get());

    if (gt != nullptr && gt->isEmpty())
        gt = nullptr;

    if (gt != nullptr)
        return gt->isParameterized();

    return false;
}

int StepClip::Channel::getIndex() const
{
    return state.getParent().indexOf (state);
}

juce::String StepClip::Channel::getDisplayName() const
{
    return name.get().isEmpty() ? juce::String (getIndex() + 1) : name;
}

//==============================================================================
juce::String StepClip::Channel::getSelectableDescription()
{
    jassertfalse;
    return {};
}

}} // namespace tracktion { inline namespace engine
