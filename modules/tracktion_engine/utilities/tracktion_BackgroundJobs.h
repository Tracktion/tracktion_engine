/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class BackgroundJobManager;

//==============================================================================
class ThreadPoolJobWithProgress  : public juce::ThreadPoolJob
{
public:
    ThreadPoolJobWithProgress (const juce::String& name)  : ThreadPoolJob (name) {}
    ~ThreadPoolJobWithProgress();

    virtual float getCurrentTaskProgress() = 0;
    virtual bool canCancel() const                  { return false; }

    void setManager (BackgroundJobManager&);

    /** Sets the job's name but also updates the manager so the list will reflect it. */
    void setName (const juce::String& newName);

    /** Call this in your sub-class destructor to to remvoe it from the manager queue before this
        class's destructor is called which can result in a pure virtual call.
    */
    void prepareForJobDeletion();

private:
    BackgroundJobManager* manager = nullptr;
};

//==============================================================================
/**
    Manages a set of background tasks that can be run concurrently on a background thread.
    This is essentially a wrapper around a ThreadPool which adds a listener interface so
    you can create UI elements to represent the list.
*/
class BackgroundJobManager  : private juce::AsyncUpdater,
                              private juce::Timer
{
public:
    BackgroundJobManager()  : pool (8)
    {
    }

    ~BackgroundJobManager()
    {
        pool.removeAllJobs (true, 30000);
    }

    void addJob (ThreadPoolJobWithProgress* job, bool takeOwnership)
    {
        if (job == nullptr)
            return;

        job->setManager (*this);
        pool.addJob (job, takeOwnership);
    }

    void removeJob (ThreadPoolJobWithProgress* job, bool interruptIfRunning, int timeOutMilliseconds)
    {
        pool.removeJob (job, interruptIfRunning, timeOutMilliseconds);
    }

    void stopAndDeleteAllRunningJobs()
    {
        // Call this twice as the first call may only stop (and not delete) running jobs
        pool.removeAllJobs (true, 30000);
        pool.removeAllJobs (true, 5000);
        jassert (pool.getNumJobs() == 0);
    }

    //==============================================================================
    struct JobInfo
    {
        JobInfo() {}

        JobInfo (ThreadPoolJobWithProgress& j, int id)
            : name (j.getJobName()), progress (j.getCurrentTaskProgress()), jobId (id), canCancel (j.canCancel()) {}

        JobInfo (const JobInfo& o)              : name (o.name), progress (o.progress), jobId (o.jobId), canCancel (o.canCancel) {}
        JobInfo& operator= (const JobInfo& o)   { name = o.name; progress = o.progress; jobId = o.jobId; canCancel = o.canCancel; return *this; }

        juce::String name;
        float progress = 0;
        int jobId = -1;
        bool canCancel = false;
    };

    void signalJobShouldExit (const JobInfo& info)
    {
        const juce::ScopedLock sl (jobsLock);

        for (int i = jobs.size(); --i >= 0;)
            if (auto pair = jobs.getUnchecked (i))
                if (pair->info.jobId == info.jobId)
                    pair->job.signalJobShouldExit();
    }

    JobInfo getJobInfo (int index) const noexcept
    {
        const juce::ScopedLock sl (jobsLock);

        if (auto j = jobs[index])
            return j->info;

        return JobInfo();
    }

    int getNumJobs() const noexcept                 { const juce::ScopedLock sl (jobsLock); return jobs.size(); }
    float getTotalProgress() const noexcept         { return totalProgress; }
    juce::ThreadPool& getPool() noexcept            { return pool; }

    //==============================================================================
    class Listener
    {
    public:
        virtual ~Listener() {}
        virtual void backgroundJobsChanged() = 0;
    };

    void addListener (Listener* l)                  { listeners.add (l); }
    void removeListener (Listener* l)               { listeners.remove (l); }

private:
    struct JobInfoPair
    {
        JobInfoPair (ThreadPoolJobWithProgress& j, int jobId)
            : job (j), info (j, jobId) {}

        ThreadPoolJobWithProgress& job;
        JobInfo info;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JobInfoPair)
    };

    friend class ThreadPoolJobWithProgress;
    juce::OwnedArray<JobInfoPair> jobs;
    juce::CriticalSection jobsLock;
    juce::ThreadPool pool;
    juce::ListenerList<Listener> listeners;
    float totalProgress = 1.0f;
    int nextJobId = 0;

    void addJobInternal (ThreadPoolJobWithProgress& job)
    {
        const juce::ScopedLock sl (jobsLock);

        jobs.add (new JobInfoPair (job, getNextJobId()));
        triggerAsyncUpdate();
        startTimerHz (25);
    }

    void removeJobInternal (ThreadPoolJobWithProgress& job)
    {
        const juce::ScopedLock sl (jobsLock);

        for (int i = jobs.size(); --i >= 0;)
            if (&jobs.getUnchecked (i)->job == &job)
                jobs.remove (i);

        triggerAsyncUpdate();
    }

    int getNextJobId() noexcept                     { return ++nextJobId &= 0xffffff; }

    void updateJobs()
    {
        if (juce::JUCEApplication::getInstance()->isInitialising())
            return;

        float newTotal = 0.0f;
        int numJobs = 0;

        {
            const juce::ScopedLock sl (jobsLock);

            for (int i = jobs.size(); --i >= 0;)
            {
                if (auto pair = jobs.getUnchecked (i))
                {
                    const float p = pair->job.getCurrentTaskProgress();
                    jassert (! std::isnan (p));
                    pair->info.progress = p;
                    newTotal += p;
                    ++numJobs;
                }
            }
        }

        newTotal = (numJobs > 0) ? (newTotal / numJobs) : 1.0f;

        if (newTotal > totalProgress)
            totalProgress = totalProgress + ((newTotal - totalProgress) / 2.0f);
        else
            totalProgress = newTotal;

        jassert (totalProgress == totalProgress
                  && juce::isPositiveAndNotGreaterThan (totalProgress, 1.0f));

        if (totalProgress >= 1.0f)
            stopTimer();
    }

    void handleAsyncUpdate() override               { listeners.call (&Listener::backgroundJobsChanged); }
    void timerCallback() override                   { updateJobs(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BackgroundJobManager)
};

inline ThreadPoolJobWithProgress::~ThreadPoolJobWithProgress()
{
    jassert (manager == nullptr); // You haven't called prepareForJobDeletion!

    if (manager != nullptr)
        manager->removeJobInternal (*this);
}

inline void ThreadPoolJobWithProgress::setManager (BackgroundJobManager& m)
{
    manager = &m;
    manager->addJobInternal (*this);
    manager->triggerAsyncUpdate();
}

inline void ThreadPoolJobWithProgress::setName (const juce::String& newName)
{
    setJobName (newName);

    if (manager != nullptr)
        manager->triggerAsyncUpdate();
}

inline void ThreadPoolJobWithProgress::prepareForJobDeletion()
{
    if (manager != nullptr)
        manager->removeJobInternal (*this);

    manager = nullptr;
}

} // namespace tracktion_engine
