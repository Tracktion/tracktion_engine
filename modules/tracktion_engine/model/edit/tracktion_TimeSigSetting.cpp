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

TimeSigSetting::TimeSigSetting (TempoSequence& ts, const juce::ValueTree& v)
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

juce::String TimeSigSetting::getSelectableDescription()
{
    return TRANS("Time Signature");
}

juce::String TimeSigSetting::getStringTimeSig() const
{
    return juce::String (numerator) + "/" + juce::String (denominator);
}

void TimeSigSetting::setStringTimeSig (const juce::String& s)
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
    ownerSequence.updateTempoDataIfNeeded();
    auto s = startTime;

    if (auto nextTimeSig = ownerSequence.getTimeSig (ownerSequence.indexOfTimeSig (this) + 1))
        return { { s, nextTimeSig->startTime }, TimeDuration() };

    return { { s, s + TimeDuration::fromSeconds (1.0) }, TimeDuration() };
}

juce::String TimeSigSetting::getName()
{
    return getStringTimeSig();
}

}} // namespace tracktion { inline namespace engine
