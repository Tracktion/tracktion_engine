/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/**
*/
class TimeSigSetting  : public TrackItem,
                        private ValueTreeAllEventListener
{
public:
    TimeSigSetting (TempoSequence&, const juce::ValueTree&);
    ~TimeSigSetting();

    using Ptr = juce::ReferenceCountedObjectPtr<TimeSigSetting>;
    using Array = juce::ReferenceCountedArray<TimeSigSetting>;

    //==============================================================================
    ClipPosition getPosition() const override;
    double getStartBeat() const                     { return startBeatNumber; }

    //==============================================================================
    // time sig in the form "4/4"
    juce::String getStringTimeSig() const;
    void setStringTimeSig (const juce::String&);

    void removeFromEdit();

    //==============================================================================
    Track* getTrack() const override;

    //==============================================================================
    juce::String getName() override;
    juce::String getSelectableDescription() override;

    //==============================================================================
    juce::ValueTree state;
    TempoSequence& ownerSequence;

    juce::CachedValue<double> startBeatNumber;
    juce::CachedValue<int> numerator, denominator;
    juce::CachedValue<bool> triplets;

    double startTime = 0; // (updated by TempoSequence)
    double endTime = 0;

private:
    void valueTreeChanged() override { changed(); };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimeSigSetting)
};

} // namespace tracktion_engine
