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
class MarkerTrack  : public ClipTrack
{
public:
    MarkerTrack (Edit&, const juce::ValueTree&);
    ~MarkerTrack();

    bool isMarkerTrack() const override;
    juce::String getSelectableDescription() override;
    juce::String getName() override;
    bool canContainPlugin (Plugin*) const override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarkerTrack)
};

} // namespace tracktion_engine
