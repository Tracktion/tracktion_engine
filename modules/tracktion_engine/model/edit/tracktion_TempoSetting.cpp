/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

TempoSetting::TempoSetting (TempoSequence& ts, const juce::ValueTree& v)
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

juce::ValueTree TempoSetting::create (double beat, double newBPM, float curveVal)
{
    return createValueTree (IDs::TEMPO,
                            IDs::startBeat, beat,
                            IDs::bpm, newBPM,
                            IDs::curve, curveVal);
}

Edit& TempoSetting::getEdit() const
{
    return ownerSequence.edit;
}

//==============================================================================
juce::String TempoSetting::getSelectableDescription()
{
    return TRANS("Tempo");
}

//==============================================================================
double TempoSetting::getStartTime() const
{
    ownerSequence.updateTempoDataIfNeeded();
    return startTime;
}
    
void TempoSetting::set (double newStartBeat, double newBPM, float newCurve, bool remapEditPositions)
{
    startBeatNumber.forceUpdateOfCachedValue();
    bpm.forceUpdateOfCachedValue();
    curve.forceUpdateOfCachedValue();

    newBPM   = juce::jlimit (minBPM, maxBPM, newBPM);
    newCurve = juce::jlimit (-1.0f, 1.0f, newCurve);

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

HashCode TempoSetting::getHash() const noexcept
{
    return static_cast<HashCode> (startBeatNumber * 128.0)
            ^ (static_cast<HashCode> (bpm * 1217.0)
                + static_cast<HashCode> (curve * 1023.0));
}

}
