/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** */
class TempoTrack  : public Track
{
public:
    TempoTrack (Edit&, const juce::ValueTree&);
    ~TempoTrack();

    using Ptr = juce::ReferenceCountedObjectPtr<TempoTrack>;

    bool isTempoTrack() const override;
    juce::String getName() override;
    juce::String getSelectableDescription() override;

    int getNumTrackItems() const override;
    TrackItem* getTrackItem (int idx) const override;
    int indexOfTrackItem (TrackItem*) const override;
    int getIndexOfNextTrackItemAt (double time) override;
    TrackItem* getNextTrackItemAt (double time) override;
    bool canContainPlugin (Plugin*) const override;

    void insertSpaceIntoTrack (double time, double amountOfSpace) override;

private:
    juce::Array<TrackItem*> buildTrackItemList() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoTrack)
};

} // namespace tracktion_engine
