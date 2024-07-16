/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
BufferedAudioReader::BufferedAudioReader (std::unique_ptr<juce::AudioFormatReader> sourceReader, juce::TimeSliceThread& t)
    : juce::AudioFormatReader (nullptr, "BufferedAudioReader"),
      source (std::move (sourceReader)), thread (t)
{
    assert (source);

    sampleRate              = source->sampleRate;
    bitsPerSample           = source->bitsPerSample;
    lengthInSamples         = source->lengthInSamples;
    numChannels             = source->numChannels;
    usesFloatingPointData   = true;
    metadataValues          = source->metadataValues;

    data.resize (choc::buffer::Size::create (numChannels, lengthInSamples));

    // Read the first chunk on the calling thread in case it needs to be played back straight away
    readNextChunk();
}

BufferedAudioReader::~BufferedAudioReader()
{
    thread.removeTimeSliceClient (this);
}

float BufferedAudioReader::getProportionComplete() const
{
    return source->lengthInSamples == 0 ? 0.0f
            : validEnd.load (std::memory_order_relaxed) / static_cast<float> (sourceLength);
}

bool BufferedAudioReader::readSamples (int* const* destSamples, int numDestChannels, int startOffsetInDestBuffer,
                                       juce::int64 startSampleInFile, int numSamples)
{
    clearSamplesBeyondAvailableLength (destSamples, numDestChannels, startOffsetInDestBuffer,
                                       startSampleInFile, numSamples, lengthInSamples);

    if (numSamples <= 0)
        return true;

    using namespace choc::buffer;
    const auto srcRange = FrameRange { static_cast<FrameCount> (startSampleInFile),
                                       static_cast<FrameCount> (startSampleInFile + numSamples) };
    const auto srcView = data.getFrameRange (srcRange);

    const auto numChannelsToRead = std::min (srcView.getNumChannels(), static_cast<FrameCount> (numDestChannels));
    const auto destSize = choc::buffer::Size::create (numChannelsToRead, numSamples);
    const auto destView = createChannelArrayView (reinterpret_cast<float* const*> (destSamples),
                                                  destSize.getChannelRange().size(),
                                                  destSize.getFrameRange().size());

    if (validEnd.load (std::memory_order_acquire) < srcRange.end)
    {
        destView.clear();
        return false;
    }

    copyIntersectionAndClearOutside (destView, srcView);

    return true;
}

int BufferedAudioReader::useTimeSlice()
{
    return readNextChunk() ? 0 : -1;
}

bool BufferedAudioReader::readNextChunk()
{
    if (! source)
        return false;

    using namespace choc::buffer;
    const auto start = validEnd.load (std::memory_order_acquire);
    const auto end = std::min (start + chunkSize, sourceLength);

    if (readIntoBuffer ({ start, end }))
    {
        validEnd.store (end, std::memory_order_release);

        if (const bool hasFinished = end == sourceLength; hasFinished)
        {
            source.reset();
            return false;
        }
    }

    return true;
}

bool BufferedAudioReader::readIntoBuffer (choc::buffer::FrameRange range)
{
    // This is just a quick way of converting the offset used by choc::buffer to a real pointer
    // The alternative would be to create some storage for the pointers and add the offsets manually
    auto destView = toAudioBuffer (data.getFrameRange (range));
    return source->read (destView.getArrayOfWritePointers(), static_cast<int> (destView.getNumChannels()),
                         static_cast<juce::int64> (range.start), static_cast<int> (range.size()));
}


//==============================================================================
BufferedAudioFileManager::BufferedAudioFileManager (Engine& e)
    : engine (e)
{
}

std::shared_ptr<BufferedAudioReader> BufferedAudioFileManager::get (juce::File f)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    auto& item = cache[f];

    if (! item)
    {
        if (auto reader = createReader (f))
        {
            item = std::make_shared<BufferedAudioReader> (std::move (reader), readThread);
            readThread.addTimeSliceClient (item.get());
            readThread.startThread (juce::Thread::Priority::normal);

            if (! timer.isTimerRunning())
                timer.startTimer (5'000);
        }
    }

    return item;
}

void BufferedAudioFileManager::cleanUp()
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    std::erase_if (cache, [](auto& item)
                  {
                      return item.second.use_count() == 1;
                  });

    if (cache.empty())
        timer.stopTimer();
}

std::unique_ptr<juce::AudioFormatReader> BufferedAudioFileManager::createReader (juce::File f)
{
    if (auto mappedFileAndReader = AudioFileUtils::createMappedFileAndReaderFor (engine, f))
        return std::make_unique<MemoryMappedFileReader> (std::move (mappedFileAndReader));

    return {};
}

}} // namespace tracktion { inline namespace engine
