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

TimeSigSetting::TimeSigSetting (TempoSequence& ts, const ValueTree& v)
    : TrackItem (ts.edit, {}, Type::timeSig),
      state (v), ownerSequence (ts)
{
    auto* um = ts.getUndoManager();

    startBeatNumber.referTo (state, IDs::startBeat, um);
    numerator.referTo (state, IDs::numerator, um, 4);
    denominator.referTo (state, IDs::denominator, um, 4);
    triplets.referTo (state, IDs::triplets, um);

    state.addListener (this);
}

TimeSigSetting::~TimeSigSetting()
{
    state.removeListener (this);

    notifyListenersOfDeletion();
}

String TimeSigSetting::getSelectableDescription()
{
    return TRANS("Time Signature");
}

String TimeSigSetting::getStringTimeSig() const
{
    return String (numerator) + "/" + String (denominator);
}

void TimeSigSetting::setStringTimeSig (const String& s)
{
    if (s.containsChar ('/'))
    {
        numerator = s.upToFirstOccurrenceOf ("/", false, false).trim().getIntValue();
        denominator = s.fromLastOccurrenceOf ("/", false, false).trim().getIntValue();
    }
}

void TimeSigSetting::removeFromEdit()
{
    jassert (Selectable::isSelectableValid (&edit));
    ownerSequence.removeTimeSig (ownerSequence.indexOfTimeSig (this));
}

Track* TimeSigSetting::getTrack() const
{
    return edit.getTempoTrack();
}

ClipPosition TimeSigSetting::getPosition() const
{
    auto s = startTime;

    if (auto nextTimeSig = ownerSequence.getTimeSig (ownerSequence.indexOfTimeSig (this) + 1))
        return { { s, nextTimeSig->startTime }, 0 };

    return { { s, s + 1.0 }, 0 };
}

String TimeSigSetting::getName()
{
    return getStringTimeSig();
}

}
