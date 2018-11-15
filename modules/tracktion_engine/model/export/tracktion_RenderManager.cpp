/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


RenderManager::Job::Job (Engine& e, const AudioFile& proxyToUse)
    : ThreadPoolJobWithProgress ("Render Job"),
      engine (e), proxy (proxyToUse)
{
    selfReference = this;

    // Adds us to the active jobs list which will start us running asynchronously to avoid
    // a pure virtual call caused by being run before we have finished being constructed
    engine.getRenderManager().addJobInternal (this);

    // We need to post a message to start our Job to avoid a pure virtual call which can
    // result if the Job is run by the thread pool before subclasses have finished being constructed
    postMessage (new UpdateMessage (UpdateMessage::started));
}

RenderManager::Job::~Job()
{
    prepareForJobDeletion();

    jassert (hasFinished);
    jassert (listeners.isEmpty());
    jassert (getReferenceCount() == 0);
}

ThreadPoolJob::JobStatus RenderManager::Job::runJob()
{
    CRASH_TRACER

    FloatVectorOperations::disableDenormalisedNumberSupport();

    if (! (isInitialised || shouldExit()))
    {
        proxy.deleteFile();
        isInitialised = true;

        if (setUpRender())
            return jobNeedsRunningAgain;
    }
    else
    {
        if (! (shouldExit() || renderNextBlock()))
            return jobNeedsRunningAgain;
    }

    const bool completedOk = completeRender();

    if (! proxy.isNull() && completedOk)
        callBlocking ([this]()
                      {
                          engine.getAudioFileManager().validateFile (proxy, true);
                          jassert (isMidiFile (proxy.getFile()) || proxy.isValid());
                      });


    const ScopedLock sl (finishedLock);

    if (! hasFinished)
        sendCompletionMessages (completedOk && (! shouldExit()));

    return jobHasFinished;
}

void RenderManager::Job::cancelJob()
{
    // We can't just signal the thread to stop as the completion messages wont get sent which
    // wont notify our listeners.
    const ScopedLock sl (finishedLock);
    signalJobShouldExit();

    if (! hasFinished)
        sendCompletionMessages (false);
}

bool RenderManager::Job::decReferenceCountWithoutDeleting() noexcept
{
    if (--refCount == 1 && selfReference != nullptr)
        cancelJob();

    // Ensures the job is deleted on the message thread
    if (refCount.get() == 0)
        engine.getRenderManager().deleteJob (this);

    return false;
}

//==============================================================================
void RenderManager::Job::sendCompletionMessages (bool success)
{
    CRASH_TRACER

    // Removes us from the active jobs list
    engine.getRenderManager().removeJobInternal (this);
    hasFinished = true;

    if (success)
    {
        postMessage (new UpdateMessage (UpdateMessage::succeded));
    }
    else
    {
        proxy.deleteFile();
        postMessage (new UpdateMessage (UpdateMessage::cancelled));
    }
}

void RenderManager::Job::handleMessage (const Message& message)
{
    CRASH_TRACER

    const UpdateMessage::Type type = UpdateMessage::getType (message);

    switch (type)
    {
        case UpdateMessage::started:
            // starts the job running
            engine.getRenderManager().addJobToPool (this);
            listeners.call (&Listener::jobStarted, *this);
            break;

        case UpdateMessage::succeded:
        case UpdateMessage::cancelled:
            if (isRunning())
            {
                const ScopedLock sl (messageLock);
                messagesToResend.addIfNotAlreadyThere (type);
                startTimer (messageRetryTimeout);
            }
            else
            {
                listeners.call (&Listener::jobFinished, *this, type == UpdateMessage::succeded);
                jassert (listeners.isEmpty());
                selfReference = nullptr;
            }
            break;

        default:
            break;
    }
}

void RenderManager::Job::timerCallback()
{
    const ScopedLock sl (messageLock);

    for (int i = messagesToResend.size(); --i >= 0;)
        postMessage (new UpdateMessage (messagesToResend.removeAndReturn (i)));
}

//==============================================================================
RenderManager::RenderManager (Engine& e) : engine (e)
{
}

RenderManager::~RenderManager()
{
    jassert (danglingJobs.isEmpty());
    jassert (jobs.isEmpty());
    jassert (jobsToDelete.isEmpty());
}

void RenderManager::cleanUp()
{
    CRASH_TRACER
    auto& pool = engine.getBackgroundJobs();

    for (int i = jobs.size(); --i >= 0;)
        pool.removeJob (jobs.getUnchecked (i), true, 10000);

    jassert (jobs.isEmpty());

    for (int i = danglingJobs.size(); --i >= 0;)
        if (auto* j = danglingJobs.getUnchecked (i))
            j->cleanUpDanglingJob();

    jassert (danglingJobs.isEmpty());

    jobs.clear();
    jobsToDelete.clear();
}

//==============================================================================
AudioFile RenderManager::getAudioFileForHash (const File& directory, int64 hash)
{
    return AudioFile (directory.getChildFile (getFileRenderPrefix() + String (hash) + ".wav"));
}

ReferenceCountedArray<RenderManager::Job> RenderManager::getRenderJobsWithoutCreating (const AudioFile& af)
{
    ReferenceCountedArray<Job> js;

    const ScopedLock sl (jobListLock);

    for (auto j : jobs)
        if (j != nullptr && j->proxy == af)
            js.add (j);

    return js;
}

int RenderManager::getNumJobs() noexcept
{
    const ScopedLock sl (jobListLock);
    return jobs.size();
}

//==============================================================================
bool RenderManager::isProxyBeingGenerated (const AudioFile& proxyFile) noexcept
{
    return findJob (proxyFile) != nullptr;
}

float RenderManager::getProportionComplete (const AudioFile& proxyFile, float defaultVal) noexcept
{
    if (Job::Ptr j = findJob (proxyFile))
        return j->progress;

    return defaultVal;
}

//==============================================================================
RenderManager::Job::Ptr RenderManager::findJob (const AudioFile& af) noexcept
{
    const ScopedLock sl (jobListLock);

    for (auto j : jobs)
        if (j != nullptr && j->proxy == af)
            return j;

    return {};
}

void RenderManager::addJobInternal (Job* j) noexcept
{
    const ScopedLock sl (jobListLock);
    jobs.addIfNotAlreadyThere (j);
}

void RenderManager::removeJobInternal (Job* j) noexcept
{
    {
        const ScopedLock sl (jobListLock);
        jobs.removeAllInstancesOf (j);
    }

    {
        const ScopedLock sl2 (deleteListLock);
        danglingJobs.addIfNotAlreadyThere (j);
    }
}

void RenderManager::addJobToPool (Job* j) noexcept
{
    engine.getBackgroundJobs().addJob (j, false);
}

void RenderManager::deleteJob (Job* j)
{
    {
        const ScopedLock sl (deleteListLock);
        danglingJobs.removeAllInstancesOf (j);
        jobsToDelete.add (j);
        triggerAsyncUpdate();
    }

    if (MessageManager::getInstance()->isThisTheMessageThread())
        handleUpdateNowIfNeeded();
}

void RenderManager::handleAsyncUpdate()
{
    const ScopedLock sl (deleteListLock);
    jobsToDelete.clear();
}
