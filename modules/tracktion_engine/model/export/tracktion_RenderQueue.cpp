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

ScopedTrackMuter::ScopedTrackMuter (Edit& edit, const juce::Array<EditItemID>& tracksToMute)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    // The list has to be de-duplicated first: muting a track twice would capture
    // the mute this class had just applied as that track's "previous" state, and
    // the restore would then leave it muted for good.
    std::vector<EditItemID> uniqueIDs (tracksToMute.begin(), tracksToMute.end());
    std::sort (uniqueIDs.begin(), uniqueIDs.end());
    uniqueIDs.erase (std::unique (uniqueIDs.begin(), uniqueIDs.end()), uniqueIDs.end());

    for (auto id : uniqueIDs)
    {
        if (auto track = findTrackForID (edit, id))
        {
            restoreList.emplace_back (track, track->isMuted (false));
            track->setMute (true);
        }
    }
}

ScopedTrackMuter::~ScopedTrackMuter()
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    for (auto& [track, wasMuted] : restoreList)
        track->setMute (wasMuted);
}

//==============================================================================
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

    // Wait for any active render to stop, then remove its partial file. A job can
    // have finished successfully and be waiting on the callAsync that clears its
    // handle, so the state has to be re-checked after the join - deleting there
    // would throw away a complete file.
    for (auto& job : jobs)
    {
        if (job->handle != nullptr)
        {
            job->handle = nullptr;   // joins the render thread, so renderSucceeded has settled

            if (! job->renderSucceeded)
                job->planned.params.destFile.deleteFile();
        }

        job->muteScope.reset();
    }
}

RenderQueue::JobPtr RenderQueue::addJob (PlannedRenderJob planned)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

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
    started = true;

    // If a job is running (or its finished callback is still in flight), the
    // queue will advance itself; starting another job now would run two at once
    for (auto& job : jobs)
        if (job->state == Job::State::active || job->handle != nullptr)
            return;

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

    // Indexed rather than a range-for, and taking a copy of the pointer: the
    // onJobStarted callback is allowed to append to the queue, which would
    // reallocate jobs and invalidate any iterator or reference into it
    for (size_t i = 0; i < jobs.size(); ++i)
    {
        auto jobPtr = jobs[i];

        if (jobPtr->state != Job::State::pending)
            continue;

        auto& job = *jobPtr;
        job.state = Job::State::active;

        if (! job.planned.tracksToMute.isEmpty() && job.planned.params.edit != nullptr)
            job.muteScope = std::make_unique<ScopedTrackMuter> (*job.planned.params.edit,
                                                                job.planned.tracksToMute);

        // An existing destination has to go before the render starts: a file output
        // stream opens at the end of whatever is already there, so rendering over a
        // file would append a second one to it. (The old exporter does this as part
        // of its overwrite confirmation.) Going through AudioFile also releases any
        // reader the file manager is holding on it.
        if (auto& destFile = job.planned.params.destFile;
            destFile.existsAsFile() && job.planned.params.edit != nullptr)
            AudioFile (job.planned.params.edit->engine, destFile).deleteFile();

        if (onJobStarted != nullptr)
            onJobStarted (job);

        // The callback may have cancelled this job. There's no handle to cancel at
        // that point, so check here rather than starting a render whose result would
        // be discarded when it finished
        if (job.state != Job::State::active)
        {
            job.muteScope.reset();
            continue;
        }

        // The finished callback arrives on the render thread, so hop back to the
        // message thread. The job pointer keeps the Job alive; the alive flag
        // skips advancing the queue if the queue itself has gone.
        job.handle = EditRenderer::render (job.planned.params,
                                           [alive = std::weak_ptr<bool> (aliveFlag), this, jobPtr] (auto renderResult)
                                           {
                                               jobPtr->renderSucceeded = renderResult.has_value();

                                               juce::MessageManager::callAsync ([alive, this, jobPtr, result = std::move (renderResult)]() mutable
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
    job.muteScope.reset();

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
