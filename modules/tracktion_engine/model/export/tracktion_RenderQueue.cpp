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

float RenderQueue::Job::getProgress() const
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (state == State::completed)
        return 1.0f;

    if (handle != nullptr)
        return handle->getProgress();

    return 0.0f;
}

void RenderQueue::Job::cancel()
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (state == State::pending)
    {
        state = State::cancelled;
    }
    else if (state == State::active)
    {
        state = State::cancelled;

        if (handle != nullptr)
            handle->cancel();
    }
}

void RenderQueue::Job::setThumbnail (std::shared_ptr<juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver> t)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    jassert (state == State::pending);
    thumbnail = std::move (t);
}

//==============================================================================
RenderQueue::~RenderQueue()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    *aliveFlag = false;
    cancelAll();

    // Wait for any active render to stop, then remove its partial file
    for (auto& job : jobs)
    {
        if (job->handle != nullptr)
        {
            job->handle = nullptr;
            job->planned.params.destFile.deleteFile();
        }
    }
}

RenderQueue::JobPtr RenderQueue::addJob (PlannedRenderJob planned)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    jassert (! started);

    jobs.push_back (JobPtr (new Job (std::move (planned))));
    return jobs.back();
}

void RenderQueue::addJobs (std::vector<PlannedRenderJob> planned)
{
    for (auto& p : planned)
        addJob (std::move (p));
}

void RenderQueue::start()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    jassert (! started);

    started = true;
    startNextJob();
}

void RenderQueue::cancelAll()
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    for (auto& job : jobs)
        job->cancel();
}

//==============================================================================
bool RenderQueue::hasFinished() const
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (! started)
        return false;

    for (auto& job : jobs)
        if (job->state == Job::State::pending || job->state == Job::State::active)
            return false;

    return true;
}

float RenderQueue::getTotalProgress() const
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (jobs.empty())
        return 0.0f;

    float total = 0.0f;

    for (auto& job : jobs)
        total += job->getProgress();

    return total / static_cast<float> (jobs.size());
}

//==============================================================================
void RenderQueue::startNextJob()
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    for (auto& jobPtr : jobs)
    {
        if (jobPtr->state != Job::State::pending)
            continue;

        auto& job = *jobPtr;
        job.state = Job::State::active;

        if (onJobStarted != nullptr)
            onJobStarted (job);

        // The finished callback arrives on the render thread, so hop back to the
        // message thread. The job pointer keeps the Job alive; the alive flag
        // skips advancing the queue if the queue itself has gone.
        job.handle = EditRenderer::render (job.planned.params,
                                           [alive = std::weak_ptr<bool> (aliveFlag), this, jobPtr] (auto result)
                                           {
                                               juce::MessageManager::callAsync ([alive, this, jobPtr, result = std::move (result)]() mutable
                                               {
                                                   if (auto stillAlive = alive.lock(); stillAlive != nullptr && *stillAlive)
                                                       handleJobFinished (*jobPtr, std::move (result));
                                               });
                                           },
                                           job.thumbnail);
        return;
    }

    if (onFinished != nullptr)
        onFinished();
}

void RenderQueue::handleJobFinished (Job& job, tl::expected<juce::File, std::string> result)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    job.handle = nullptr;

    // A job cancelled while active has already been marked cancelled
    if (job.state == Job::State::active)
    {
        if (result)
        {
            job.state = Job::State::completed;
            job.resultFile = *result;
        }
        else
        {
            job.state = Job::State::failed;
            job.error = juce::String (result.error());
        }
    }

    // Failed and cancelled jobs mustn't leave partial files behind
    if (job.state != Job::State::completed)
        job.planned.params.destFile.deleteFile();

    if (onJobFinished != nullptr)
        onJobFinished (job);

    startNextJob();
}

} // namespace tracktion::inline engine
