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

/** */
class MarkerTrack  : public ClipTrack
{
public:
    MarkerTrack (Edit&, const juce::ValueTree&);
    ~MarkerTrack() override;

    bool isMarkerTrack() const override;
    juce::String getSelectableDescription() override;
    juce::String getName() override;
    bool canContainPlugin (Plugin*) const override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MarkerTrack)
};

}} // namespace tracktion { inline namespace engine
