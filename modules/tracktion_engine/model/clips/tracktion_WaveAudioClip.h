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
class WaveAudioClip  : public AudioClipBase
{
public:
    WaveAudioClip (const juce::ValueTree&, EditItemID, ClipTrack&);
    ~WaveAudioClip();

    using Ptr = juce::ReferenceCountedObjectPtr<WaveAudioClip>;

    //==============================================================================
    void initialise() override;
    void cloneFrom (Clip*) override;

    //==============================================================================
    juce::String getSelectableDescription() override;

    bool isMidi() const override                                { return false; }
    bool usesSourceFile() override                              { return true; }

    double getSourceLength() const override;
    void sourceMediaChanged() override;

    juce::File getOriginalFile() const override;
    juce::int64 getHash() const override;

    void setLoopDefaults() override;

    //==============================================================================
    void addTake (ProjectItemID);
    bool hasAnyTakes() const override                           { return getTakesTree().getNumChildren() > 0; }
    int getNumTakes (bool includeComps) override;

    juce::Array<ProjectItemID> getTakes() const override;
    void clearTakes() override;
    int getCurrentTake() const override;
    void setCurrentTake (int takeIndex) override;
    bool isCurrentTakeComp() override;

    juce::StringArray getTakeDescriptions() const override;
    void deleteAllUnusedTakes (bool deleteSourceFiles);
    void deleteAllUnusedTakesConfirmingWithUser (bool deleteSourceFiles);
    Clip::Array unpackTakes (bool toNewTracks) override;

    static juce::String getCompPrefix()         { return "comp_"; }
    AudioFile getCompFileFor (juce::int64 takeHash) const;
    WaveCompManager& getCompManager();

    void reassignReferencedItem (const ReferencedItem&, ProjectItemID newID, double newStartTime) override;

    //==============================================================================
    bool needsRender() const override;
    RenderManager::Job::Ptr getRenderJob (const AudioFile& destFile) override;
    juce::String getRenderMessage() override;
    void renderComplete() override;

    bool isUsingFile (const AudioFile& af) override;

private:
    //==============================================================================
    mutable double sourceLength = 0;
    WaveCompManager::Ptr compManager;

    static constexpr int takeIndexNeedsUpdating = -2;
    mutable int currentTakeIndex = takeIndexNeedsUpdating;

    juce::ValueTree getTakesTree() const                        { return state.getChildWithName (IDs::TAKES); }
    void invalidateCurrentTake() noexcept;
    void invalidateCurrentTake (const juce::ValueTree&) noexcept;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveAudioClip)
};

} // namespace tracktion_engine
