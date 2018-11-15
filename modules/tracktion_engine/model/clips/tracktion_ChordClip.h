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
class ChordClip   : public Clip,
                    private juce::AsyncUpdater
{
public:
    ChordClip (const juce::ValueTree&, EditItemID, ClipTrack& targetTrack);
    ~ChordClip();

    juce::String getSelectableDescription() override;
    void setTrack (ClipTrack*) override;
    bool canGoOnTrack (Track&) override;
    juce::Colour getColour() const override;
    juce::Colour getDefaultColour() const override;
    void initialise() override;

    bool isMidi() const override         { return false; }
    bool isMuted() const override        { return false; }

    AudioNode* createAudioNode (const CreateAudioNodeParams&) override      { return {}; }

    PatternGenerator* getPatternGenerator() override;
    void pitchTempoTrackChanged() override;

protected:
    void handleAsyncUpdate() override;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree& p, juce::ValueTree& c) override;
    void valueTreeChildRemoved (juce::ValueTree& p, juce::ValueTree& c, int) override;
    void valueTreeParentChanged (juce::ValueTree& treeWhoseParentHasChanged) override;

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordClip)
};

} // namespace tracktion_engine
