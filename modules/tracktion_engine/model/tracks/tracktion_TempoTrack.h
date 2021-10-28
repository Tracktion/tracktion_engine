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
    int getIndexOfNextTrackItemAt (double time) override;
    /** @internal */
    TrackItem* getNextTrackItemAt (double time) override;
    /** @internal */
    bool canContainPlugin (Plugin*) const override;

    /** @internal */
    void insertSpaceIntoTrack (double time, double amountOfSpace) override;

private:
    juce::Array<TrackItem*> buildTrackItemList() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoTrack)
};

} // namespace tracktion_engine
