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
    This is the main source of an Edit clip and is responsible for managing its
    properties.
*/
class EditClip    : public AudioClipBase,
                    private EditSnapshot::Listener
{
public:
    //==============================================================================
    EditClip (const juce::ValueTree&, EditItemID, ClipTrack&, ProjectItemID sourceEdit);
    ~EditClip() override;

    using Ptr = juce::ReferenceCountedObjectPtr<EditClip>;

    /** Returns the AudioFileInfo for an edit clip.
        This is a bit of a hack but necessary to get the AudioSegmentList structures
        to work correctly.
    */
    AudioFileInfo getWaveInfo()  override               { return waveInfo; }

    /** Returns the cached EditSnapshot that represents the current state of the source. */
    EditSnapshot::Ptr getEditSnapshot() noexcept        { return editSnapshot; }

    AudioFile getAudioFile() const override;

    //==============================================================================
    void initialise() override;
    void cloneFrom (Clip*) override;

    bool isMidi() const override                        { return false; }
    bool canHaveEffects() const override                { return false; }

    double getCurrentStretchRatio() const;

    //==============================================================================
    juce::String getSelectableDescription() override;

    juce::File getOriginalFile() const override;
    HashCode getHash() const override                   { return hash; }

    void setLoopDefaults() override;

    void setTracksToRender (const juce::Array<EditItemID>& trackIDs);

    //==============================================================================
    bool needsRender() const override;
    RenderManager::Job::Ptr getRenderJob (const AudioFile& destFile) override;
    void renderComplete() override;
    juce::String getRenderMessage() override;
    juce::String getClipMessage() override;

    //==============================================================================
    TimeDuration getSourceLength() const override       { return editSnapshot == nullptr ? 0_td : editSnapshot->getLength(); }
    bool usesSourceFile() override                      { return false; }
    void sourceMediaChanged() override;
    void changed() override;

    bool isUsingFile (const AudioFile& af) override;

    RenderOptions& getRenderOptions() const noexcept    { jassert (renderOptions != nullptr); return *renderOptions; }

    ProjectItem::Ptr createUniqueCopy();
    HashCode generateHash();

    juce::CachedValue<bool> copyColourFromMarker, trimToMarker, renderEnabled;

protected:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

private:
    //==============================================================================
    AsyncCaller sourceIdUpdater;

    //==============================================================================
    ProjectItemID lastSourceId;
    EditSnapshot::Ptr editSnapshot;
    juce::ReferenceCountedArray<EditSnapshot> referencedEdits;

    AudioFileInfo waveInfo;
    HashCode hash = 0;
    std::unique_ptr<RenderOptions> renderOptions;
    bool sourceMediaReEntrancyCheck = false;

    //==============================================================================
    void updateWaveInfo();
    void updateReferencedEdits();
    void updateLoopInfoBasedOnSource (bool updateLength);

    void editChanged (EditSnapshot&) override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditClip)
};

}} // namespace tracktion { inline namespace engine
