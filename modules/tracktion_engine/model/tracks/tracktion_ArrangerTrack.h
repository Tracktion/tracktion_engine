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

/** A track similar to the MarkerTrack but can be used to move sections of an Edit around.
*/
class ArrangerTrack  : public ClipTrack
{
public:
    ArrangerTrack (Edit&, const juce::ValueTree&);
    ~ArrangerTrack() override;

    bool isArrangerTrack() const override;
    juce::String getSelectableDescription() override;
    juce::String getName() override;
    bool canContainPlugin (Plugin*) const override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArrangerTrack)
};

}} // namespace tracktion { inline namespace engine
