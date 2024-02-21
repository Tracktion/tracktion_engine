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

//==============================================================================
/**
*/
class ChordClip   : public Clip,
                    private juce::AsyncUpdater
{
public:
    ChordClip (const juce::ValueTree&, EditItemID, ClipOwner& targetParent);
    ~ChordClip() override;

    juce::String getSelectableDescription() override;
    bool canBeAddedTo (ClipOwner&) override;
    juce::Colour getColour() const override;
    juce::Colour getDefaultColour() const override;
    void initialise() override;

    bool isMidi() const override         { return false; }
    bool isMuted() const override        { return false; }

    PatternGenerator* getPatternGenerator() override;
    void pitchTempoTrackChanged() override;

protected:
    void handleAsyncUpdate() override;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeParentChanged (juce::ValueTree&) override;

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordClip)
};

}} // namespace tracktion { inline namespace engine
