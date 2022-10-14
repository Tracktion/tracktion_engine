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

//==============================================================================
/**
*/
class TimeSigSetting  : public TrackItem,
                        private ValueTreeAllEventListener
{
public:
    TimeSigSetting (TempoSequence&, const juce::ValueTree&);
    ~TimeSigSetting() override;

    using Ptr = juce::ReferenceCountedObjectPtr<TimeSigSetting>;
    using Array = juce::ReferenceCountedArray<TimeSigSetting>;

    //==============================================================================
    ClipPosition getPosition() const override;
    BeatPosition getStartBeat() const                     { return startBeatNumber; }

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

    juce::CachedValue<BeatPosition> startBeatNumber;
    juce::CachedValue<int> numerator, denominator;
    juce::CachedValue<bool> triplets;

    TimePosition startTime; // (updated by TempoSequence)
    TimePosition endTime;

private:
    void valueTreeChanged() override { changed(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimeSigSetting)
};

}} // namespace tracktion { inline namespace engine
