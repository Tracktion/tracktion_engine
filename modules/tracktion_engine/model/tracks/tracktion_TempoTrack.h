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
    A track to represent the master plugins.
    This isn't a "real" track, it wraps the master plugin list.
*/
class MasterTrack   : public Track
{
public:
    MasterTrack (Edit&, const juce::ValueTree&);
    ~MasterTrack() override;

    using Ptr = juce::ReferenceCountedObjectPtr<MasterTrack>;

    void initialise() override;
    bool isMasterTrack() const override;
    juce::String getName() override;
    juce::String getSelectableDescription() override;

    bool canContainPlugin (Plugin*) const override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterTrack)
};

} // namespace tracktion_engine
