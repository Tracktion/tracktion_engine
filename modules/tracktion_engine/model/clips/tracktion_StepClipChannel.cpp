/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


StepClip::Channel::Channel (StepClip& c, const ValueTree& v)
    : clip (c), state (v)
{
    auto* um = &clip.edit.getUndoManager();

    channel.referTo (state, IDs::channel, um, MidiChannel (defaultMidiChannel));
    noteNumber.referTo (state, IDs::note, um, defaultNoteNumber);
    noteValue.referTo (state, IDs::velocity, um, defaultNoteValue);
    grooveTemplate.referTo (state, IDs::groove, um);
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
            && name == other.name;
}

int StepClip::Channel::getIndex() const
{
    return state.getParent().indexOf (state);
}

String StepClip::Channel::getDisplayName() const
{
    return name.get().isEmpty() ? String (getIndex() + 1) : name;
}

//==============================================================================
String StepClip::Channel::getSelectableDescription()
{
    jassertfalse;
    return {};
}
