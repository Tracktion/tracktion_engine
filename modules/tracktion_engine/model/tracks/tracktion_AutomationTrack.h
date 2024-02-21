/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
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
    juce::String getName() const override;

    //==============================================================================
    using Ptr = juce::ReferenceCountedObjectPtr<AutomationTrack>;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutomationTrack)
};

}} // namespace tracktion { inline namespace engine
