/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


PitchSetting::PitchSetting (Edit& ed, const ValueTree& v)
    : TrackItem (ed, {}, Type::pitch), state (v)
{
    auto um = &ed.getUndoManager();

    startBeat.referTo (state, IDs::startBeat, um);
    pitch.referTo (state, IDs::pitch, um, 60);
    scale.referTo (state, IDs::scale, um, Scale::major);

    state.addListener (this);
}

PitchSetting::~PitchSetting()
{
    state.removeListener (this);

    notifyListenersOfDeletion();
}

String PitchSetting::getName()
{
    return MidiMessage::getMidiNoteName (pitch, true, false, edit.engine.getEngineBehaviour().getMiddleCOctave());
}

String PitchSetting::getSelectableDescription()
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
    auto s = edit.tempoSequence.beatsToTime (startBeat);

    if (auto nextPitch = ps.getPitch (ps.indexOfPitch (this) + 1))
        return { { s, nextPitch->getPosition().getStart() }, 0 };

    return { { s, s + 1.0 }, 0 };
}

void PitchSetting::setStartBeat (double beat)
{
    startBeat = beat;
    edit.pitchSequence.sortEvents();
    changed();
}

void PitchSetting::setPitch (int p)
{
    pitch = jlimit (0, 127, p);
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
