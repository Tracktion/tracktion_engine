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
