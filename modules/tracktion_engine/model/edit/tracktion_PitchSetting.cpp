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

PitchSetting::PitchSetting (Edit& ed, const juce::ValueTree& v)
    : TrackItem (ed, {}, Type::pitch), state (v)
{
    auto um = &ed.getUndoManager();

    startBeat.referTo (state, IDs::startBeat, um);
    pitch.referTo (state, IDs::pitch, um, 60);
    accidentalsSharp.referTo (state, IDs::accidentalsSharp, um, true);
    scale.referTo (state, IDs::scale, um, Scale::major);

    state.addListener (this);
}

PitchSetting::~PitchSetting()
{
    state.removeListener (this);

    notifyListenersOfDeletion();
}

juce::String PitchSetting::getName()
{
    return juce::MidiMessage::getMidiNoteName (pitch, accidentalsSharp, false,
                                               edit.engine.getEngineBehaviour().getMiddleCOctave());
}

juce::String PitchSetting::getSelectableDescription()
{
    return TRANS("Key");
}

Track* PitchSetting::getTrack() const
{
    return edit.getTempoTrack();
}

ClipPosition PitchSetting::getPosition() const
{
    auto& ps = edit.pitchSequence;
    auto s = edit.tempoSequence.toTime (startBeat);

    if (auto nextPitch = ps.getPitch (ps.indexOfPitch (this) + 1))
        return { { s, nextPitch->getPosition().getStart() }, TimeDuration() };

    return { { s, s + TimeDuration::fromSeconds (1.0) }, TimeDuration() };
}

void PitchSetting::setStartBeat (BeatPosition beat)
{
    startBeat = beat;
    edit.pitchSequence.sortEvents();
    changed();
}

void PitchSetting::setPitch (int p)
{
    pitch = juce::jlimit (0, 127, p);
}

void PitchSetting::setScaleID (Scale::ScaleType st)
{
    scale = st;
}

void PitchSetting::removeFromEdit()
{
    auto p = state.getParent();

    if (p.getNumChildren() > 1)
        p.removeChild (state, &edit.getUndoManager());
}

}} // namespace tracktion { inline namespace engine
