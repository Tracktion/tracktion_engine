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
class EditRenderJob   : public RenderManager::Job
{
public:
    //==============================================================================
    /** Returns a job that will have been started to generate the Render described by the params.
        To be notified of when the job completes add yourself as a listener. If the deleteEdit argument
        is true this will delete the Edit once it is done with it. The destFile should be the same as the
        params.destFile argument and is only provided so the file doesn't get deleted before creation.
        This job will continue to run until all references to it are deleted. Once this happens the
        render will be abandoned. If you delete yourself, make sure to unregister as a listener too.
    */
    static Ptr getOrCreateRenderJob (Engine&, Renderer::Parameters&, bool deleteEdit,
                                     bool silenceOnBackup, bool reverse);

    /** Creates a job based on a set of RenderOptions properties and a source Edit ProjectItemID.
        If this method is used the Edit will be created on the background render thread and be owned by the job.
    */
    static Ptr getOrCreateRenderJob (Engine&, const AudioFile& destFile, const RenderOptions&, ProjectItemID itemID,
                                     bool silenceOnBackup, bool reverse);

    //==============================================================================
    /** Destructor. */
    ~EditRenderJob();

    /** Returns the parameters in use for this job. */
    const Renderer::Parameters& getParams() const noexcept  { return params; }

    /** Returns an audio thumbnail that will be updated with the progress of the render
        operation. Don't hang onto this however as it is owned by the job and you should
        create your own thumbnail from the file once the job has finished.
    */
    juce::AudioThumbnail& getAudioThumbnail()               { return thumbnailToUpdate; }

    /** Returns true if this job is rendering MIDI.
        This can be used to determine if the AudioThumbnail should be used to generate
        a thumbnail or some other means.
    */
    bool isMidi() const noexcept                            { return params.createMidiFile; }

    /** Returns the result of this render. Only valid once the job has completed. */
    Renderer::RenderResult& getResult()                     { return result; }

    juce::String getLastError() const;

protected:
    //==============================================================================
    bool setUpRender() override;
    bool renderNextBlock() override;
    bool completeRender() override;
    void setLastError (const juce::String&);

private:
    //==============================================================================
    struct RenderPass
    {
        RenderPass (EditRenderJob&, Renderer::Parameters&, const juce::String& description);
        ~RenderPass();

        bool initialise();

        EditRenderJob& owner;
        Renderer::Parameters r;
        const juce::String desc;
        ProjectItem::Category originalCategory;
        juce::TemporaryFile tempFile;
        juce::ScopedPointer<Renderer::RenderTask> task;
    };

    RenderOptions renderOptions;
    const ProjectItemID itemID;

    Renderer::Parameters params;
    juce::OptionalScopedPointer<Edit> editDeleter;
    juce::OwnedArray<RenderPass> renderPasses;
    bool silenceOnBackup, reverse;
    Renderer::RenderResult result;

    juce::String lastError;
    juce::CriticalSection errorLock;

    juce::AudioThumbnail thumbnailToUpdate;

    void renderSeparateTracks();
    bool generateSilence (const juce::File& fileToWriteTo);

    //==============================================================================
    EditRenderJob (Engine&, const Renderer::Parameters&, bool deleteEdit, bool silenceOnBackup, bool reverse);

    EditRenderJob (Engine&, const AudioFile& destFile, const RenderOptions&, ProjectItemID itemID,
                   bool silenceOnBackup, bool reverse);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditRenderJob)
};

} // namespace tracktion_engine
