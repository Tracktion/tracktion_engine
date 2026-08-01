/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine
{

//==============================================================================
/**
    Mutes a set of tracks for its lifetime, restoring their previous mute
    states on destruction. Message-thread only.

    Render jobs pair this with Renderer::Parameters::tracksToProcessWhileMuted
    so the muted tracks keep feeding sidechains, aux buses and racks in the
    render graph while their direct output is silenced.
*/
class ScopedTrackMuter
{
public:
    ScopedTrackMuter (Edit&, const juce::Array<EditItemID>& tracksToMute);
    ~ScopedTrackMuter();

private:
    std::vector<std::pair<Track::Ptr, bool>> restoreList;

    JUCE_DECLARE_NON_COPYABLE (ScopedTrackMuter)
};

//==============================================================================
/**
    Runs a list of render jobs strictly one at a time, in order.

    Add jobs (usually from expandRenderSpecification()), keeping hold of the
    returned Job handles, attach any callbacks, then call start(). Each job
    renders on its own background thread via EditRenderer; the next job only
    starts once the previous one has finished. More jobs can be appended while
    the queue is running - call start() again after adding them in case the
    earlier jobs have already finished. All methods and callbacks are
    message-thread only.

    Deleting the queue cancels any remaining jobs. Job handles remain valid
    after the queue has been deleted.
*/
class RenderQueue
{
public:
    //==============================================================================
    /**
        A single job in the queue, obtained from addJob()/addJobs()/getJobs().
        Holds everything about the job: its parameters, state, progress and result.
    */
    class Job
    {
    public:
        //==============================================================================
        enum class State
        {
            pending,        ///< Not started yet
            active,         ///< Currently rendering
            completed,      ///< Finished successfully
            failed,         ///< Finished with an error, see getError()
            cancelled       ///< Cancelled before or during rendering
        };

        //==============================================================================
        State getState() const                                  { return state; }
        juce::String getName() const                            { return planned.name; }

        /** The parameters this job renders with. */
        const Renderer::Parameters& getParameters() const       { return planned.params; }

        /** Returns this job's progress, 0 to 1. */
        float getProgress() const;

        /** Returns the rendered file once the job has completed. */
        juce::File getFile() const                              { return resultFile; }

        /** Returns the error message if the job failed. */
        juce::String getError() const                           { return error; }

        //==============================================================================
        /** Cancels this job. Pending jobs are skipped, an active job is stopped. */
        void cancel();

        /** Sets a receiver (e.g. a juce::AudioThumbnail) to be filled while this
            job renders. Only valid while the job is still pending.
        */
        void setThumbnail (std::shared_ptr<juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver>);

        /** Returns the receiver set with setThumbnail(), if any. */
        std::shared_ptr<juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver> getThumbnail() const { return thumbnail; }

    private:
        //==============================================================================
        friend class RenderQueue;

        explicit Job (PlannedRenderJob p) : planned (std::move (p)) {}

        PlannedRenderJob planned;
        State state = State::pending;
        juce::File resultFile;
        juce::String error;
        std::shared_ptr<juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver> thumbnail;
        std::shared_ptr<EditRenderer::Handle> handle;

        /** Set on the render thread the moment the render returns, before the
            message-thread hop that updates state. ~RenderQueue can run in between
            the two and needs to know whether destFile is a complete file. */
        std::atomic<bool> renderSucceeded { false };

        /** Held while the job renders; releasing it restores the muted source
            tracks. The Job owning it means the mutes can't outlive the job
            even if a handle outlives the queue. */
        std::unique_ptr<ScopedTrackMuter> muteScope;

        JUCE_DECLARE_NON_COPYABLE (Job)
    };

    using JobPtr = std::shared_ptr<Job>;

    //==============================================================================
    RenderQueue() = default;

    /** Destructor. Cancels any active and pending jobs. */
    ~RenderQueue();

    //==============================================================================
    /** Adds a job to the queue, returning its handle. Jobs may be appended at
        any time, including while the queue is running - call start() again
        afterwards in case the earlier jobs have already finished. */
    JobPtr addJob (PlannedRenderJob);

    /** Adds a list of jobs to the queue. */
    void addJobs (std::vector<PlannedRenderJob>);

    /** Starts running the queued jobs in order. Safe to call again after
        appending jobs to a running or finished queue: a no-op while a job is
        active, otherwise it picks up the next pending job. onFinished fires
        each time the queue runs out of pending jobs. */
    void start();

    /** Cancels the active job and all pending jobs. */
    void cancelAll();

    //==============================================================================
    /** Returns the jobs in the queue, in execution order. */
    const std::vector<JobPtr>& getJobs() const                  { return jobs; }

    bool hasStarted() const                                     { return started; }

    /** Returns true once every job has completed, failed or been cancelled. */
    bool hasFinished() const;

    /** Returns the overall progress of the queue, 0 to 1. */
    float getTotalProgress() const;

    //==============================================================================
    std::function<void (Job&)> onJobStarted;    ///< Called on the message thread as each job starts
    std::function<void (Job&)> onJobFinished;   ///< Called on the message thread as each job finishes
    std::function<void()> onFinished;           ///< Called on the message thread once all jobs have finished

private:
    //==============================================================================
    std::vector<JobPtr> jobs;
    bool started = false;
    std::shared_ptr<bool> aliveFlag { std::make_shared<bool> (true) };

    void startNextJob();
    void handleJobFinished (Job&, tl::expected<juce::File, std::string>);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenderQueue)
};

} // namespace tracktion::inline engine
