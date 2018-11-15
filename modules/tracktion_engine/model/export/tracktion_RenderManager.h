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
    Manages a set of reference counted render jobs and can be used to retrieve
    matching jobs or create new ones.
*/
class RenderManager   : private juce::DeletedAtShutdown,
                        private juce::AsyncUpdater
{
public:
    //==============================================================================
    /**
        The base class that all generator jobs derive from. This provides access to the
        jobs progress and also a listener interface so subclasses can be notified when
        a job is started or completed.
    */
    class Job  : public ThreadPoolJobWithProgress,
                 private juce::MessageListener,
                 private juce::Timer
    {
    public:
        //==============================================================================
        using Ptr = juce::ReferenceCountedObjectPtr<Job>;

        virtual ~Job();

        /** Performs the render. */
        JobStatus runJob() override;

        /** Returns the progress of the job. */
        float getCurrentTaskProgress() override             { return progress; }

        /** Cancels the current job safely making sure any listeners are called appropriately. */
        void cancelJob();

        /** Called during app shutdown by the manager on any jobs that haven't had a chance to
            recieve their async completion callbacks.
         */
        void cleanUpDanglingJob()                           { selfReference = nullptr; }

        //==============================================================================
        void incReferenceCount() noexcept                   { ++refCount; }
        bool decReferenceCountWithoutDeleting() noexcept;
        int getReferenceCount() const noexcept              { return refCount.get(); }

        //==============================================================================
        struct Listener
        {
            virtual ~Listener() {}
            virtual void jobStarted (RenderManager::Job&) {}
            virtual void jobFinished (RenderManager::Job&, bool completedOk) = 0;
        };

        void addListener (Listener* l)                      { listeners.add (l); }
        void removeListener (Listener* l)                   { listeners.remove (l); }

        //==============================================================================
        Engine& engine;
        AudioFile proxy;
        std::atomic<float> progress { 0 };

        /** Subclasses should override this to set-up their render process.
            Return true if the set-up completed successfully and the rest of the render callbacks
            should be called, false if there was a problem and the render should be stopped.
         */
        virtual bool setUpRender() = 0;

        /** During a render process this will be repeatedly called.
            Return true once all the blocks have completed, false if this needs to be called again.
         */
        virtual bool renderNextBlock() = 0;

        /** This is called once after all the render blocks have completed.
            Subclasses should override this to finish off their render by closing files and etc.
            returning true if everything completed successfully, false otherwise.
         */
        virtual bool completeRender() = 0;

    protected:
        //==============================================================================
        Job (Engine&, const AudioFile& proxy); // don't instantiate directly!

    private:
        //==============================================================================
        struct UpdateMessage  : public juce::Message
        {
            enum Type
            {
                started,
                succeded,
                cancelled,
                unknown
            };

            UpdateMessage (Type t) : type (t) {}

            static Type getType (const juce::Message& message)
            {
                if (auto um = dynamic_cast<const UpdateMessage*> (&message))
                    return um->type;

                return unknown;
            }

            Type type;

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateMessage)
        };

        //==============================================================================
        enum { messageRetryTimeout = 50 };

        juce::ListenerList<Listener> listeners;
        juce::Atomic<int> refCount;
        Ptr selfReference;
        juce::CriticalSection messageLock, finishedLock;
        juce::Array<UpdateMessage::Type> messagesToResend;
        bool isInitialised = false, hasFinished = false;

        void sendCompletionMessages (bool success);

        void handleMessage (const juce::Message&) override;
        void timerCallback() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Job)
    };

    //==============================================================================
    RenderManager (Engine&);
    ~RenderManager();

    /** Cleans up any remaining jobs - should be called before the manager is deleted. */
    void cleanUp();

    //==============================================================================
    /** Returns the AudioFile for a particular hash. If this is not valid you should
        then start a new job using getOrCreateRenderJob.
        You should always check this first, never start a new job unnecessarily.
    */
    static AudioFile getAudioFileForHash (const juce::File& directory, juce::int64 hash);

    /** This will return a Ptr to an existing render job for an audio file or nullptr if no job is in progress. */
    Job::Ptr getRenderJobWithoutCreating (const AudioFile& audioFile)       { return findJob (audioFile); }

    /** Returns all the jobs that may be processing the given file. */
    juce::ReferenceCountedArray<Job> getRenderJobsWithoutCreating (const AudioFile&);

    /** Returns the number of jobs in the pool. */
    int getNumJobs() noexcept;

    //==============================================================================
    /** Returns the prefix used for render files. */
    static juce::StringRef getFileRenderPrefix()      { return "render_"; }

    /** Returns true if a render is currently being performed for this AudioFile. */
    bool isProxyBeingGenerated (const AudioFile& proxyFile) noexcept;

    /** Returns true if a render is currently being performed for this AudioFile. */
    float getProportionComplete (const AudioFile& proxyFile, float defaultVal) noexcept;

    Engine& engine;

private:
    //==============================================================================
    juce::Array<Job*> jobs, danglingJobs;
    juce::OwnedArray<Job> jobsToDelete;
    juce::CriticalSection jobListLock, deleteListLock;

    Job::Ptr findJob (const AudioFile&) noexcept;
    void addJobInternal (Job*) noexcept;
    void removeJobInternal (Job*) noexcept;
    void addJobToPool (Job*) noexcept;
    void deleteJob (Job*);

    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenderManager)
};

} // namespace tracktion_engine
