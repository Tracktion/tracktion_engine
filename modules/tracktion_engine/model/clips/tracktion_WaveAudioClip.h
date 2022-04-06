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
    An audio clip that uses an audio file as its source.
*/
class WaveAudioClip  : public AudioClipBase
{
public:
    /** Creates a WaveAudioClip from a given state. @see ClipTrack::insertWaveClip. */
    WaveAudioClip (const juce::ValueTree&, EditItemID, ClipTrack&);
    
    /** Destructor. */
    ~WaveAudioClip() override;

    using Ptr = juce::ReferenceCountedObjectPtr<WaveAudioClip>;

    //==============================================================================
    /** Adds a new take with the ProjectItemID as the source. */
    void addTake (ProjectItemID);
    
    /** Adds a new take with the given file as the source. */
    void addTake (const juce::File&);

    /** Deletes all but the current takes.
        @param deleteSourceFiles    If true, also deletes the source files
    */
    void deleteAllUnusedTakes (bool deleteSourceFiles);

    /** Deletes all but the current takes but shows a confirmation dialog first.
        @param deleteSourceFiles    If true, also deletes the source files
    */
    void deleteAllUnusedTakesConfirmingWithUser (bool deleteSourceFiles);

    /** Returns the WaveCompManager for this clip. */
    WaveCompManager& getCompManager();

    //==============================================================================
    /** @internal */
    bool needsRender() const override;
    /** @internal */
    RenderManager::Job::Ptr getRenderJob (const AudioFile& destFile) override;
    /** @internal */
    juce::String getRenderMessage() override;
    /** @internal */
    void renderComplete() override;

    /** @internal */
    bool isUsingFile (const AudioFile&) override;

    //==============================================================================
    /** @internal */
    void initialise() override;
    /** @internal */
    void cloneFrom (Clip*) override;

    //==============================================================================
    /** @internal */
    juce::String getSelectableDescription() override;

    /** @internal */
    bool isMidi() const override                                { return false; }
    /** @internal */
    bool usesSourceFile() override                              { return true; }

    /** @internal */
    TimeDuration getSourceLength() const override;
    /** @internal */
    void sourceMediaChanged() override;

    /** @internal */
    juce::File getOriginalFile() const override;
    /** @internal */
    HashCode getHash() const override;

    /** @internal */
    void setLoopDefaults() override;

    //==============================================================================
    /** @internal */
    juce::StringArray getTakeDescriptions() const override;
    /** @internal */
    bool hasAnyTakes() const override                           { return getTakesTree().getNumChildren() > 0; }
    /** @internal */
    int getNumTakes (bool includeComps) override;
    /** @internal */
    juce::Array<ProjectItemID> getTakes() const override;
    /** @internal */
    void clearTakes() override;
    /** @internal */
    int getCurrentTake() const override;
    /** @internal */
    void setCurrentTake (int takeIndex) override;
    /** @internal */
    bool isCurrentTakeComp() override;
    /** @internal */
    Clip::Array unpackTakes (bool toNewTracks) override;

    /** @internal */
    void reassignReferencedItem (const ReferencedItem&, ProjectItemID newID, double newStartTime) override;

private:
    //==============================================================================
    mutable TimeDuration sourceLength;
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

}} // namespace tracktion { inline namespace engine
