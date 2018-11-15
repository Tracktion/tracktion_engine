/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


TempoSetting::TempoSetting (TempoSequence& ts, const ValueTree& v)
    : ReferenceCountedObject(),
      ownerSequence (ts), state (v)
{
    auto* um = ts.getUndoManager();

    startBeatNumber.referTo (state, IDs::startBeat, um);
    bpm.referTo (state, IDs::bpm, um, 120.0);
    curve.referTo (state, IDs::curve, um);
}

TempoSetting::~TempoSetting()
{
}

ValueTree TempoSetting::create (double beat, double newBPM, float curveVal)
{
    ValueTree v (IDs::TEMPO);
    v.setProperty (IDs::startBeat, beat, nullptr);
    v.setProperty (IDs::bpm, newBPM, nullptr);
    v.setProperty (IDs::curve, curveVal, nullptr);
    return v;
}

void TempoSetting::selectionStatusChanged (bool)
{
    if (editor != nullptr)
        editor->repaint();
}

Edit& TempoSetting::getEdit() const
{
    return ownerSequence.edit;
}

//==============================================================================
String TempoSetting::getSelectableDescription()
{
    return TRANS("Tempo");
}

//==============================================================================
void TempoSetting::set (double newStartBeat, double newBPM, float newCurve, bool remapEditPositions)
{
    startBeatNumber.forceUpdateOfCachedValue();
    bpm.forceUpdateOfCachedValue();
    curve.forceUpdateOfCachedValue();

    newBPM = jlimit (minBPM, maxBPM, newBPM);
    newCurve = jlimit (-1.0f, 1.0f, newCurve);

    if (newBPM != bpm || startBeatNumber != newStartBeat || curve != newCurve)
    {
        EditTimecodeRemapperSnapshot snap;

        if (remapEditPositions)
            snap.savePreChangeState (getEdit());

        bpm = newBPM;
        curve = newCurve;
        startBeatNumber = newStartBeat;

        changed();

        if (remapEditPositions)
            snap.remapEdit (getEdit());
    }
}

void TempoSetting::setStartBeat (double newStartBeat)
{
    set (newStartBeat, bpm, curve, false);
}

void TempoSetting::setBpm (double newBpm)
{
    set (startBeatNumber, newBpm, curve, true);
}

void TempoSetting::setCurve (float newCurve)
{
    set (startBeatNumber, bpm, newCurve, true);
}

double TempoSetting::getApproxBeatLength() const
{
    return 240.0 / (bpm * getMatchingTimeSig().denominator);
}

void TempoSetting::removeFromEdit()
{
    CRASH_TRACER
    ownerSequence.removeTempo (ownerSequence.indexOfTempo (this), true);
}

TempoSetting* TempoSetting::getPreviousTempo() const
{
    return ownerSequence.getTempo (ownerSequence.indexOfTempo (this) - 1);
}

TimeSigSetting& TempoSetting::getMatchingTimeSig() const
{
    return ownerSequence.getTimeSigAtBeat (startBeatNumber);
}

int64 TempoSetting::getHash() const noexcept
{
    return (int64) (startBeatNumber * 128.0) ^ (int64) (bpm * 1217.0) + (int64) (curve * 1023.0);
}
