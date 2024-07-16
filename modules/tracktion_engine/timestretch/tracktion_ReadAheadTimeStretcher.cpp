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
//==============================================================================
ReadAheadTimeStretcher::ProcessThread::ProcessThread()
{
    thread = std::thread ([this] { process(); });
}

ReadAheadTimeStretcher::ProcessThread::~ProcessThread()
{
    waitingToExitFlag.test_and_set();
    event.signal();
    thread.join();
}

void ReadAheadTimeStretcher::ProcessThread::addInstance (ReadAheadTimeStretcher* instance)
{
    breakForAddRemoveInstance.test_and_set();

    const std::unique_lock sl (instancesMutex);
    instances.emplace_back (instance);

    breakForAddRemoveInstance.clear();
}

void ReadAheadTimeStretcher::ProcessThread::removeInstance (ReadAheadTimeStretcher* instance)
{
    breakForAddRemoveInstance.test_and_set();

    const std::unique_lock sl (instancesMutex);
    std::erase_if (instances, [&] (auto& i) { return i == instance; });

    breakForAddRemoveInstance.clear();
}

void ReadAheadTimeStretcher::ProcessThread::flagForProcessing (std::atomic<std::uint64_t>& epoch)
{
    epoch = processEpoch.load (std::memory_order_acquire);
    event.signal();
}

//==============================================================================
void ReadAheadTimeStretcher::ProcessThread::process()
{
    for (;;)
    {
        if (waitingToExitFlag.test (std::memory_order_acquire))
            return;

        bool anyInstancesProcessed = false;

        {
            const std::unique_lock sl (instancesMutex);
            const auto currentEpoch = processEpoch.load (std::memory_order_acquire);

            for (auto instance : instances)
            {
                if (waitingToExitFlag.test (std::memory_order_acquire))
                    return;

                if (breakForAddRemoveInstance.test (std::memory_order_acquire))
                {
                    anyInstancesProcessed = false; // Run again asap
                    break;
                }

                // Skip unflagged instances
                if (instance->getEpoch() > currentEpoch)
                    continue;

                if (instance->processNextBlock (false))
                     anyInstancesProcessed = true;
            }
        }

        processEpoch.fetch_add (1, std::memory_order_release);

        if (! anyInstancesProcessed)
            event.wait (-1);
    }
}


//==============================================================================
//==============================================================================
ReadAheadTimeStretcher::ReadAheadTimeStretcher (int numBlocksToReadAhead_)
    : numBlocksToReadAhead (numBlocksToReadAhead_)
{
}

ReadAheadTimeStretcher::~ReadAheadTimeStretcher()
{
    processThread->removeInstance (this);
}

void ReadAheadTimeStretcher::initialise (double sourceSampleRate, int samplesPerBlock,
                                         int numChannelsToUse, TimeStretcher::Mode mode, TimeStretcher::ElastiqueProOptions proOpts,
                                         bool realtime)
{
    assert (! stretcher.isInitialised() && "Can only initialise once");

    numSamplesPerOutputBlock = samplesPerBlock;
    numChannels = numChannelsToUse;

    stretcher.initialise (sourceSampleRate, samplesPerBlock,
                          numChannelsToUse, mode, proOpts,
                          realtime);

    if (! isInitialised())
        return;

    processThread->addInstance (this);
    inputFifo.setSize (numChannels, getMaxFramesNeeded());
    outputFifo.setSize (numChannels, samplesPerBlock * numBlocksToReadAhead);
}

bool ReadAheadTimeStretcher::isInitialised() const
{
    return stretcher.isInitialised();
}

void ReadAheadTimeStretcher::reset()
{
    const std::scoped_lock sl (processMutex);

    inputFifo.reset();
    outputFifo.reset();
    newSpeedAndPitchPending.store (true, std::memory_order_release);

    stretcher.reset();
    tryToSetNewSpeedAndPitch();

    hasBeenReset.store (true, std::memory_order_release);
}

bool ReadAheadTimeStretcher::setSpeedAndPitch (float speedRatio, float semitonesUp)
{
    const bool setSpeed = ! juce::approximatelyEqual (pendingSpeedRatio.exchange (speedRatio, std::memory_order_acq_rel), speedRatio);
    const bool setPitch = ! juce::approximatelyEqual (pendingSemitonesUp.exchange (semitonesUp, std::memory_order_acq_rel), semitonesUp);

    if (setSpeed || setPitch)
        newSpeedAndPitchPending.store (true, std::memory_order_release);

    return true;
}

int ReadAheadTimeStretcher::getMaxFramesNeeded() const
{
    // N.B. This should be thread safe as its constant in the stretcher implementations
    return stretcher.getMaxFramesNeeded();
}

int ReadAheadTimeStretcher::getFramesNeeded() const
{
    const std::scoped_lock sl (processMutex);
    return stretcher.getFramesNeeded();
}

int ReadAheadTimeStretcher::getFramesRecomended() const
{
    // The logic here is complicated but basically we want to avoid reading too many
    // samples at once as it's expensive to read and resample them.
    // However, after the first reset, we do want to provide enough samples to get
    // the first block out. After that, we want to keep the stretcher stoked with
    // samples so the background thread can process them
    return hasBeenReset.load (std::memory_order_acquire) ? getFramesNeeded()
                                                         : std::min (std::max (getFramesNeeded(), numSamplesPerOutputBlock * 2),
                                                                     getFreeSpace());
}

bool ReadAheadTimeStretcher::requiresMoreFrames() const
{
    return inputFifo.getNumReady() < getFramesNeeded();
}

int ReadAheadTimeStretcher::getFreeSpace() const
{
    return inputFifo.getFreeSpace();
}

int ReadAheadTimeStretcher::pushData (const float* const* inChannels, int numSamples)
{
    assert (inputFifo.getFreeSpace() >= numSamples);
    inputFifo.write (inChannels, numSamples);
    processThread->flagForProcessing (epoch);
    hasBeenReset.store (false, std::memory_order_release);

    return numSamples;
}

int ReadAheadTimeStretcher::getNumReady() const
{
    return outputFifo.getNumReady();
}

int ReadAheadTimeStretcher::popData (float* const* outChannels, int numSamples)
{
    assert (numSamples <= numSamplesPerOutputBlock || numSamples <= getNumReady());

    if (outputFifo.getNumReady() <= numSamples)
    {
        [[ maybe_unused ]] const int numPopped = processNextBlock (true);
        assert (numPopped > 0 && "Not enough input frames pushed");
    }

    const int numToRead = std::min (numSamples, outputFifo.getNumReady());
    juce::AudioBuffer<float> destBuffer (outChannels, numChannels, numToRead);
    outputFifo.read (destBuffer, 0);
    return numToRead;
}

int ReadAheadTimeStretcher::flush (float* const* outChannels)
{
    {
        AudioScratchBuffer scratchBuffer (numChannels, numSamplesPerOutputBlock);
        const int numFramesReturned = stretcher.flush ((float **) scratchBuffer.buffer.getArrayOfWritePointers());
        assert (numFramesReturned <= scratchBuffer.buffer.getNumSamples());
        assert (outputFifo.getFreeSpace() >= numFramesReturned);
        outputFifo.write (scratchBuffer.buffer, 0, numFramesReturned);
    }

    // If enough samples are ready, output these now
    if (const int numReady = outputFifo.getNumReady(); numReady > 0)
    {
        const int numToReturn = std::min (numReady, numSamplesPerOutputBlock);
        juce::AudioBuffer<float> outputView (outChannels, numChannels, numToReturn);
        [[maybe_unused]] const bool success = outputFifo.read (outputView, 0);
        jassert (success);

        return outputView.getNumSamples();
    }

    return 0;
}

void ReadAheadTimeStretcher::tryToSetNewSpeedAndPitch() const
{
    if (! newSpeedAndPitchPending.exchange (false, std::memory_order_acq_rel))
        return;

    stretcher.setSpeedAndPitch (pendingSpeedRatio.load (std::memory_order_acquire),
                                pendingSemitonesUp.load (std::memory_order_acquire));
}

int ReadAheadTimeStretcher::processNextBlock (bool block)
{
    if (outputFifo.getFreeSpace() < numSamplesPerOutputBlock)
        return 0;

    std::unique_lock<std::mutex> sl;

    if (block)
        sl = std::unique_lock (processMutex);
    else
        sl = std::unique_lock (processMutex, std::try_to_lock);

    if (! sl.owns_lock())
        return 0;

    // If we were waiting for the lock to be released, there might now be samples ready for us so don't process again
    if (block)
        if (outputFifo.getNumReady() >= numSamplesPerOutputBlock)
            return numSamplesPerOutputBlock;

    if (inputFifo.getNumReady() < stretcher.getFramesNeeded())
        return 0;

    tryToSetNewSpeedAndPitch();
    return stretcher.processData (inputFifo, stretcher.getFramesNeeded(), outputFifo);
}

}