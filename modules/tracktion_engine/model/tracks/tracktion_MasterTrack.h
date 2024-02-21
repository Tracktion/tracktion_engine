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

/**
    A track to represent the master plugins.
    This isn't a "real" track, it wraps the master plugin list.
*/
class MasterTrack   : public Track
{
public:
    /** Create the MasterTrack for an Edit with a given state. */
    MasterTrack (Edit&, const juce::ValueTree&);

    /** Destructor. */
    ~MasterTrack() override;

    using Ptr = juce::ReferenceCountedObjectPtr<MasterTrack>;

    /** @internal */
    void initialise() override;
    /** @internal */
    bool isMasterTrack() const override;
    /** @internal */
    juce::String getName() const override;
    /** @internal */
    juce::String getSelectableDescription() override;

    /** @internal */
    bool canContainPlugin (Plugin*) const override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterTrack)
};

}} // namespace tracktion { inline namespace engine
