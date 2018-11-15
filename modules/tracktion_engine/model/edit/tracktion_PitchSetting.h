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
class PitchSetting   : public TrackItem,
                       private ValueTreeAllEventListener
{
public:
    PitchSetting (Edit&, const juce::ValueTree&);
    ~PitchSetting();

    using Ptr = juce::ReferenceCountedObjectPtr<PitchSetting>;
    using Array = juce::ReferenceCountedArray<PitchSetting>;

    juce::String getSelectableDescription() override;

    double getStartBeatNumber() const       { return startBeat; }
    void setStartBeat (double);

    int getPitch() const                    { return pitch; }
    void setPitch (int);

    Scale::ScaleType getScale() const       { return scale; }
    void setScaleID (Scale::ScaleType);

    void removeFromEdit();

    juce::String getName() override;
    Track* getTrack() const override;
    ClipPosition getPosition() const override;

    juce::ValueTree state;
    juce::CachedValue<double> startBeat;
    juce::CachedValue<int> pitch;
    juce::CachedValue<Scale::ScaleType> scale;

private:
    void valueTreeChanged() override { changed(); };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchSetting)
};

} // namespace tracktion_engine
