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
class AutomationTrack  : public Track
{
public:
    AutomationTrack (Edit&, const juce::ValueTree&);
    ~AutomationTrack() override;

    //==============================================================================
    bool isAutomationTrack() const override            { return true; }
    juce::String getSelectableDescription() override;
    bool canContainPlugin (Plugin*) const override     { return false; }
    juce::String getName() override;

    //==============================================================================
    using Ptr = juce::ReferenceCountedObjectPtr<AutomationTrack>;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationTrack)
};

}} // namespace tracktion { inline namespace engine
