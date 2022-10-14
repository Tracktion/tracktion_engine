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

/**
    A track to represent the "global" items such as tempo, key changes etc.
    This isn't a "real" track, it wraps the TempoSequence and PitchSequence.
*/
class TempoTrack  : public Track
{
public:
    /** Create the TempoTrack for an Edit with a given state. */
    TempoTrack (Edit&, const juce::ValueTree&);
    
    /** Destructor. */
    ~TempoTrack() override;

    using Ptr = juce::ReferenceCountedObjectPtr<TempoTrack>;

    /** @internal */
    bool isTempoTrack() const override;
    /** @internal */
    juce::String getName() override;
    /** @internal */
    juce::String getSelectableDescription() override;

    /** @internal */
    int getNumTrackItems() const override;
    /** @internal */
    TrackItem* getTrackItem (int idx) const override;
    /** @internal */
    int indexOfTrackItem (TrackItem*) const override;
    /** @internal */
    int getIndexOfNextTrackItemAt (TimePosition) override;
    /** @internal */
    TrackItem* getNextTrackItemAt (TimePosition) override;
    /** @internal */
    bool canContainPlugin (Plugin*) const override;

    /** @internal */
    void insertSpaceIntoTrack (TimePosition, TimeDuration) override;

private:
    juce::Array<TrackItem*> buildTrackItemList() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoTrack)
};

}} // namespace tracktion { inline namespace engine
