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

BufferedFileReader::BufferedFileReader (juce::AudioFormatReader* sourceReader,
                                        juce::TimeSliceThread& timeSliceThread,
                                        int samplesToBuffer)
    : juce::AudioFormatReader (nullptr, sourceReader->getFormatName()),
      source (sourceReader), thread (timeSliceThread),
      isFullyBuffering (samplesToBuffer < 0)
{
    static_assert (std::atomic<BufferedBlock*>::is_always_lock_free);
    static_assert (std::atomic<bool>::is_always_lock_free);

    sampleRate            = source->sampleRate;
    lengthInSamples       = source->lengthInSamples;
    numChannels           = source->numChannels;
    metadataValues        = source->metadataValues;
    bitsPerSample         = 32;
    usesFloatingPointData = true;

    const size_t totalNumSlotsRequired = 1 + (size_t (lengthInSamples) / samplesPerBlock);
    assert (totalNumSlotsRequired <= std::numeric_limits<int>::max());
    numBlocksToBuffer = samplesToBuffer > -1 ? static_cast<size_t> (1 + (samplesToBuffer / samplesPerBlock))
                                             : totalNumSlotsRequired;

    slots = std::vector<std::atomic<BufferedBlock*>> (totalNumSlotsRequired);
    std::fill (slots.begin(), slots.end(), nullptr);

    slotsInUse = std::vector<std::atomic<bool>> (totalNumSlotsRequired);
    std::fill (slotsInUse.begin(), slotsInUse.end(), false);

    for (size_t i = 0; i < numBlocksToBuffer; ++i)
    {
        // The following code makes the assumption that the pointers are at least 8-bit aligned
        static_assert (alignof (BufferedBlock*) >= 8);
        blocks.push_back (std::make_unique<BufferedBlock> (*source));

        // Check the least significant bit is actually 0
        assert (! std::bitset<sizeof (BufferedBlock*)> (size_t (blocks.back().get()))[0]);
    }

    timeSliceThread.addTimeSliceClient (this);
}

BufferedFileReader::~BufferedFileReader()
{
    thread.removeTimeSliceClient (this);
}

void BufferedFileReader::setReadTimeout (int timeoutMilliseconds) noexcept
{
    timeoutMs = timeoutMilliseconds;
}

bool BufferedFileReader::isFullyBuffered() const
{
    return isFullyBuffering
        && numBlocksBuffered == numBlocksToBuffer;
}

bool BufferedFileReader::readSamples (int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                                      juce::int64 startSampleInFile, int numSamples)
{
    // If a cached block can't be found, update the read position and possibly wait for it
    nextReadPosition = startSampleInFile;

    auto startTime = juce::Time::getMillisecondCounter();
    clearSamplesBeyondAvailableLength (destSamples, numDestChannels, startOffsetInDestBuffer,
                                       startSampleInFile, numSamples, lengthInSamples);

    bool allSamplesRead = true;
    bool hasNotified = false;

    while (numSamples > 0)
    {
        {
            auto ssa = ScopedSlotAccess::fromPosition (*this, startSampleInFile);

            if (auto block = ssa.getBlock())
            {
                jassert (block->range.contains (startSampleInFile));

                // This isn't exact but will be ok for finding the oldest block
                block->lastUseTime = startTime;

                auto offset = (int) (startSampleInFile - block->range.getStart());
                auto numToDo = std::min (numSamples, (int) (block->range.getEnd() - startSampleInFile));

                for (int j = 0; j < numDestChannels; ++j)
                {
                    if (auto dest = (float*) destSamples[j])
                    {
                        dest += startOffsetInDestBuffer;

                        if (j < (int) numChannels)
                            juce::FloatVectorOperations::copy (dest, block->buffer.getReadPointer (j, offset), numToDo);
                        else
                            juce::FloatVectorOperations::clear (dest, numToDo);
                    }
                }

                startOffsetInDestBuffer += numToDo;
                startSampleInFile += numToDo;
                numSamples -= numToDo;

                allSamplesRead = allSamplesRead && block->allSamplesRead;

                // Use a continue here rather an an else to avoid keeping the ScopedSlotAccess in scope
                continue;
            }
        }

        if (! std::exchange (hasNotified, true))
            thread.moveToFrontOfQueue (this);

        // If the timeout has expired, clear the dest buffer and return
        if (timeoutMs >= 0 && juce::Time::getMillisecondCounter() >= startTime + (juce::uint32) timeoutMs)
        {
            for (int j = 0; j < numDestChannels; ++j)
                if (auto dest = (float*) destSamples[j])
                    juce::FloatVectorOperations::clear (dest + startOffsetInDestBuffer, numSamples);

            allSamplesRead = false;
            break;
        }
        else
        {
            // Otherwise wait and try again
            juce::Thread::yield();
        }
    }

    return allSamplesRead;
}

BufferedFileReader::BufferedBlock::BufferedBlock (juce::AudioFormatReader& reader)
    : buffer ((int) reader.numChannels, samplesPerBlock)
{
}

void BufferedFileReader::BufferedBlock::update (juce::AudioFormatReader& reader, juce::Range<juce::int64> newSampleRange, size_t currentSlotIndex)
{
    assert (newSampleRange.getEnd() <= reader.lengthInSamples);
    const int numSamples = (int) newSampleRange.getLength();
    range = newSampleRange;
    buffer.setSize ((int) reader.numChannels,
                    numSamples,
                    false, false, true);
    allSamplesRead = reader.read (&buffer, 0, numSamples, (int) range.getStart(), true, true);
    assert (slotIndex == static_cast<int> (currentSlotIndex));
    slotIndex = static_cast<int> (currentSlotIndex);
    lastUseTime = juce::Time::getMillisecondCounter();
    DBG(slotIndex);
}

BufferedFileReader::ScopedSlotAccess::ScopedSlotAccess (BufferedFileReader& reader_, size_t slotIndex_)
    : reader (reader_), slotIndex (slotIndex_)
{
    assert (slotIndex < reader.slots.size());

    reader.markSlotUseState (slotIndex, true);
    block = reader.slots[slotIndex];
}

BufferedFileReader::ScopedSlotAccess::~ScopedSlotAccess()
{
    block = nullptr;
    reader.markSlotUseState (slotIndex, false);
}

BufferedFileReader::ScopedSlotAccess BufferedFileReader::ScopedSlotAccess::fromPosition (BufferedFileReader& reader, juce::int64 position)
{
    return { reader, static_cast<size_t> (position / (float) samplesPerBlock) };
}

void BufferedFileReader::ScopedSlotAccess::setBlock (BufferedBlock* blockToReferTo)
{
    if (block)
        block->slotIndex = -1;

    block = blockToReferTo;

    if (block)
        block->slotIndex = static_cast<int> (slotIndex);

    reader.slots[slotIndex] = block;
}

int BufferedFileReader::useTimeSlice()
{
    for (;;)
    {
        switch (readNextBufferChunk())
        {
            case PositionStatus::positionChangedByAudioThread:  break;
            case PositionStatus::nextChunkScheduled:            return 1;
            case PositionStatus::blocksFull:                    return 5;
            case PositionStatus::fullyLoaded:                   return 100;
        }
    }
}

BufferedFileReader::PositionStatus BufferedFileReader::readNextBufferChunk()
{
    if (isFullyBuffered())
        return PositionStatus::fullyLoaded;

    // First find the slot the audio thread is trying to read
    // If that needs reading, set the next-slot-to-read to this
    // If it's already read, use the current next-slot-to-read value
    const auto currentReadPosition = nextReadPosition.load();
    const auto currentSlotIndex = getSlotIndexFromSamplePosition (currentReadPosition);

    // Check if the slot being read by the audio thread is buffered
    {
        // Take exclusive control of the current slot
        ScopedSlotAccess currentSlot (*this, currentSlotIndex);

        if (auto currentBlockInCurrentSlot = currentSlot.getBlock())
        {
            // If the slot has a valid block, it should have the correct range
            assert (currentBlockInCurrentSlot->range == getSlotRange (currentSlotIndex));

            // If the block contains invalid data, we need to re-read this
            if (! currentBlockInCurrentSlot->allSamplesRead)
                nextSlotScheduled = currentSlotIndex;
        }
    }

    const auto slotToReadIndex = nextSlotScheduled.load();
    bool readScheduledSlot = true;

    {
        // Take exclusive control of the scheduled slot
        ScopedSlotAccess scheduledSlot (*this, slotToReadIndex);

        // If that block is inthe correct slot with all the samples read, don't re-read
        if (auto currentBlockInScheduledSlot = scheduledSlot.getBlock())
            if (currentBlockInScheduledSlot->allSamplesRead)
                readScheduledSlot = false;
    }

    // Read the next scheduled slot if its out of date
    if (readScheduledSlot)
    {
        // This can be done without taking any exclusive access as the
        // blocks won't move around and their time stamps are atomic
        juce::uint32 oldestTime = std::numeric_limits<juce::uint32>::max();
        BufferedBlock* blockToUse = nullptr;

        for (size_t i = 0; i < blocks.size(); ++i)
        {
            auto& block = blocks[i];
            const auto useTime = block->lastUseTime.load (std::memory_order_relaxed);

            if (useTime > oldestTime)
                continue;

            blockToUse = block.get();
            oldestTime = useTime;

            const int curentBlockSlotIndex = block->slotIndex.load (std::memory_order_relaxed);

            // If the block is free, we can simply use it
            if (curentBlockSlotIndex < 0)
                break;
        }

        assert (blockToUse != nullptr);
        const int blockToUseSlotIndex = blockToUse->slotIndex.load (std::memory_order_relaxed);
        ScopedSlotAccess desiredSlot (*this, slotToReadIndex);

        if (blockToUseSlotIndex < 0)
        {
            // If the block isn't in use, just set its slot
            desiredSlot.setBlock (blockToUse);
        }
        else
        {
            // Take exclusive control of the slot the oldest block is in
            ScopedSlotAccess slotWithOldestBlock (*this, static_cast<size_t> (blockToUseSlotIndex));
            assert (blockToUse == slotWithOldestBlock.getBlock());

            // Swap the blocks
            slotWithOldestBlock.setBlock (nullptr);
            desiredSlot.setBlock (blockToUse);
        }

        // Update the block's data
        const juce::Range<juce::int64> slotSampleRange = getSlotRange (slotToReadIndex);
        blockToUse->update (*source, slotSampleRange, slotToReadIndex);

        // Increment the number of blocks in use if this was a fresh block
        if (blockToUseSlotIndex < 0 && blockToUse->allSamplesRead)
            ++numBlocksBuffered;
    }

    // If all the slots are in use, free the oldest slot from the current read position so a future one can be queued up
//ddd    if (numBlocksBuffered == blocks.size() && slotToReadIndex > 0)
//    {
//        for (auto& block : blocks)
//        {
//            // This won't change during this function
//            const auto blockSlotIndex = block->slotIndex.load();
//
//            if (blockSlotIndex < 0)
//                continue;
//
//            if (blockSlotIndex < int (slotToReadIndex) - 1)
//            {
//                ScopedSlotAccess slotAccess (*this, size_t (blockSlotIndex));
//                slotAccess.setBlock (nullptr);
//                --numBlocksBuffered;
//            }
//        }
//
//        return PositionStatus::nextChunkScheduled;
//    }

    // If the read position hasn't changed by the audio thread let the thread reschedule us
    if (nextReadPosition == currentReadPosition)
    {
        assert (blocks.size() > 0);
        const auto startSlot = std::max (size_t (0), getSlotIndexFromSamplePosition (currentReadPosition));
        nextSlotScheduled = (startSlot + 1) % slots.size();

        return PositionStatus::nextChunkScheduled;
    }

    // Otherwise, the audio thread has changed the position so read the block asap
    return PositionStatus::positionChangedByAudioThread;
}

size_t BufferedFileReader::getSlotIndexFromSamplePosition (juce::int64 samplePos)
{
    return static_cast<size_t> (samplePos / (float) samplesPerBlock);
}

juce::Range<juce::int64> BufferedFileReader::getSlotRange (size_t slotIndex)
{
    const juce::int64 slotStartSamplePos = static_cast<juce::int64> (slotIndex * samplesPerBlock);
    const juce::int64 slotEndSamplePos = std::min (slotStartSamplePos + samplesPerBlock, lengthInSamples);

    return { slotStartSamplePos, slotEndSamplePos };
}

void BufferedFileReader::markSlotUseState (size_t slotIndex, bool isInUse)
{
    assert (slotIndex < slotsInUse.size());
    auto& slotInUseState = slotsInUse[slotIndex];
    auto expected = ! isInUse;

    for (;;)
    {
        if (slotInUseState.compare_exchange_weak (expected, isInUse))
            break;

        expected = ! isInUse; // Update this as it might have been changed by the cmp call!
    }
}

}} // namespace tracktion { inline namespace engine
