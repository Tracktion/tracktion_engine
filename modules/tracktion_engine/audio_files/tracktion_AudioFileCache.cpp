/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


static void clearSetOfChannels (int** channels, int numChannels, int offset, int numSamples) noexcept
{
    for (int i = 0; i < numChannels; ++i)
        if (auto chan = (float*) channels[i])
            juce::FloatVectorOperations::clear (chan + offset, numSamples);
}

class AudioFileCache::CachedFile
{
public:
    CachedFile (AudioFileCache& c, const AudioFile& f)
        : cache (c), file (f), info (f.getInfo())
    {
       #if ! JUCE_64BIT
        if (info.lengthInSamples <= cache.cacheSizeSamples)
       #endif
            mapEntireFile = true;
    }

    enum { readAheadSamples = 48000 };

    void touchFiles()
    {
        juce::Array<juce::int64> readPoints;
        readPoints.ensureStorageAllocated (64);

        {
            const juce::ScopedReadLock sl (clientListLock);

            for (auto r : clients)
            {
                const auto readPos = r->readPos.load();
                const auto loopLength = r->loopLength.load();

                if (r->getReferenceCount() > 1 && readPos > -readAheadSamples)
                {
                    if (loopLength > 0)
                        if (readPos + readAheadSamples > r->loopStart + loopLength)
                            readPoints.addIfNotAlreadyThere (r->loopStart);

                    readPoints.addIfNotAlreadyThere (std::max ((juce::int64) 0, readPos));
                }
            }
        }

        const juce::ScopedReadLock sl (readerLock);

        for (auto pos : readPoints)
            touchAllReaders ({ pos, pos + 128 });

        for (auto pos : readPoints)
            touchAllReaders ({ pos + 128, pos + 4096 });

        for (int distanceAhead = 4096; distanceAhead < 48000; distanceAhead += 8192)
            for (auto pos : readPoints)
                touchAllReaders ({ pos + distanceAhead, pos + distanceAhead + 8192 });
    }

    void touchAllReaders (juce::Range<juce::int64> range) const
    {
        for (auto* r : readers)
        {
            if (r != nullptr)
            {
                range = range.getIntersectionWith (r->getMappedSection());

                for (auto i = range.getStart(); i < range.getEnd(); i += 64)
                    r->touchSample (i);
            }
        }
    }

    bool updateBlocks()
    {
        if (mapEntireFile && readers.size() > 0)
            return false;

        const juce::ScopedLock scl (blockUpdateLock);

        if (mapEntireFile)
        {
            if (readers.isEmpty())
            {
                if (failedToOpenFile
                     && juce::Time::getApproximateMillisecondCounter()
                            < lastFailedOpenAttempt + 4000 + (juce::uint32) juce::Random::getSystemRandom().nextInt (3000))
                    return false;

                const juce::ScopedWriteLock sl (readerLock);

                if (auto r = createNewReader (nullptr))
                {
                    readers.add (r);
                }
                else
                {
                    failedToOpenFile = true;
                    lastFailedOpenAttempt = juce::Time::getMillisecondCounter();
                }
            }

            return false;
        }

        bool anythingChanged = false;
        bool needToPurgeUnusedClients = false;
        auto blockSize = cache.cacheSizeSamples;
        auto lastPossibleBlockIndex = (int) ((info.lengthInSamples - 1) / blockSize);
        juce::Array<int> blocksNeeded;

        {
            const juce::ScopedReadLock sl (clientListLock);

            for (auto r : clients)
            {
                if (r->getReferenceCount() <= 1)
                {
                    needToPurgeUnusedClients = true;
                }
                else
                {
                    const auto readPos = r->readPos.load();
                    const auto loopStart = r->loopStart.load();
                    const auto loopLength = r->loopLength.load();

                    if (loopLength > 0)
                    {
                        auto loopEnd = loopStart + loopLength;
                        auto start = std::max (0, (int) (std::max (loopStart, readPos - 256) / blockSize));
                        auto end   = std::min (lastPossibleBlockIndex, (int) (std::min (loopEnd, readPos + readAheadSamples) / blockSize));

                        for (int i = start; i <= end; ++i)
                            blocksNeeded.addIfNotAlreadyThere (i);

                        if (readPos + readAheadSamples > loopEnd)
                            blocksNeeded.addIfNotAlreadyThere ((int) (loopStart / blockSize));
                    }
                    else
                    {
                        auto start = std::max (0, (int) ((readPos - 256) / blockSize));
                        auto end   = std::min (lastPossibleBlockIndex, (int) ((readPos + readAheadSamples) / blockSize));

                        for (int i = start; i <= end; ++i)
                            blocksNeeded.addIfNotAlreadyThere (i);
                    }
                }
            }
        }

        if (blocksNeeded != currentBlocks)
        {
            juce::OwnedArray<juce::MemoryMappedAudioFormatReader> newReaders;

            {
                const juce::ScopedReadLock sl (readerLock);

                for (int i = 0; i < blocksNeeded.size(); ++i)
                {
                    const int block = blocksNeeded.getUnchecked(i);
                    const int existingIndex = currentBlocks.indexOf (block);

                    juce::MemoryMappedAudioFormatReader* newReader;

                    if (existingIndex >= 0)
                    {
                        newReader = readers.getUnchecked (existingIndex);
                        readers.set (existingIndex, nullptr, false);
                    }
                    else
                    {
                        auto pos = block * (juce::int64) blockSize;
                        const juce::Range<juce::int64> range (pos, pos + blockSize);
                        newReader = createNewReader (&range);
                    }

                    if (newReader != nullptr)
                        newReaders.add (newReader);
                    else
                        blocksNeeded.remove (i--);
                }
            }

            {
                const juce::ScopedWriteLock sl (readerLock);
                newReaders.swapWith (readers);
                currentBlocks.swapWith (blocksNeeded);

                jassert (readers.size() == currentBlocks.size());
            }

            for (auto m : newReaders)
                if (m != nullptr)
                    totalBytesInUse -= m->getNumBytesUsed();

            anythingChanged = true;
        }

        if (needToPurgeUnusedClients)
        {
            const juce::ScopedWriteLock sl (clientListLock);

            for (int i = clients.size(); --i >= 0;)
                if (clients.getObjectPointerUnchecked(i)->getReferenceCount() <= 1)
                    clients.remove (i);
        }

        return anythingChanged;
    }

    void dumpBlocks()
    {
        for (int i = 0; i < currentBlocks.size(); ++i)
            DBG ("File " << file.getFile().getFileName() << "    " << currentBlocks[i]
                 << "  " << readers[i]->getMappedSection().getStart()
                 << " - " << readers[i]->getMappedSection().getEnd());
    }

    juce::MemoryMappedAudioFormatReader* createNewReader (const juce::Range<juce::int64>* range)
    {
        juce::AudioFormat* af;
        std::unique_ptr<juce::MemoryMappedAudioFormatReader> r (AudioFileUtils::createMemoryMappedReader (file.getFile(), af));

        if (r != nullptr
             && (range != nullptr ? r->mapSectionOfFile (*range)
                                  : r->mapEntireFile())
             && ! r->getMappedSection().isEmpty())
        {
            totalBytesInUse += r->getNumBytesUsed();
            failedToOpenFile = false;

            info = AudioFileInfo (file, r.get(), af);
            return r.release();
        }

        return {};
    }

    void purgeOrphanReaders()
    {
        const juce::ScopedWriteLock sl (clientListLock);

        for (int i = clients.size(); --i >= 0;)
            if (clients.getObjectPointerUnchecked (i)->getReferenceCount() <= 1)
                clients.remove (i);
    }

    bool isUnused() const
    {
        return clients.isEmpty();
    }

    void releaseReader()
    {
        const juce::ScopedWriteLock sl (readerLock);
        readers.clear();
        currentBlocks.clear();
    }

    void validateFile()
    {
        const juce::ScopedLock scl (blockUpdateLock);
        failedToOpenFile = false;
    }

    struct LockedReaderFinder
    {
        LockedReaderFinder (CachedFile& f, juce::int64 startSample, int timeoutMs)  : lock (f.readerLock)
        {
            juce::uint32 startTime = 0;

            for (;;)
            {
                if (lock.tryEnterRead())
                {
                    reader = f.findReaderFor (startSample);

                    if (reader != nullptr)
                        return;

                    lock.exitRead();
                }

                if (timeoutMs < 0)
                {
                    if (startTime != 0) // second failed after calling updateBlocks failed
                        break;

                    f.updateBlocks();
                    startTime = 1;
                    continue;
                }

                if (timeoutMs == 0)
                    break;

                auto now = juce::Time::getMillisecondCounter();

                if (startTime == 0)
                    startTime = now;

                const int elapsed = (int) (now - startTime);

                if (elapsed > timeoutMs)
                    break;

                if (elapsed > 0)
                    juce::Thread::yield();
            }

            isLocked = false;
        }

        ~LockedReaderFinder()
        {
            if (isLocked)
                lock.exitRead();
        }

        juce::MemoryMappedAudioFormatReader* reader;
        juce::ReadWriteLock& lock;
        bool isLocked = true;

        JUCE_DECLARE_NON_COPYABLE (LockedReaderFinder)
    };

    bool read (juce::int64 startSample, int** destSamples, int numDestChannels,
               int startOffsetInDestBuffer, int numSamples, int timeoutMs)
    {
        jassert (destSamples != nullptr);
        jassert (startSample >= 0);

        bool allDataRead = true;

        while (numSamples > 0)
        {
            if (startSample >= info.lengthInSamples)
            {
                clearSetOfChannels (destSamples, numDestChannels, startOffsetInDestBuffer, numSamples);
                break;
            }

            const LockedReaderFinder l (*this, startSample, timeoutMs);
            SCOPED_REALTIME_CHECK

            if (l.isLocked && l.reader != nullptr)
            {
                auto numThisTime = std::min (numSamples, (int) (l.reader->getMappedSection().getEnd() - startSample));

                l.reader->readSamples (destSamples, numDestChannels, startOffsetInDestBuffer, startSample, numThisTime);

                startSample += numThisTime;
                startOffsetInDestBuffer += numThisTime;
                numSamples -= numThisTime;
            }
            else
            {
                allDataRead = false;
                clearSetOfChannels (destSamples, numDestChannels, startOffsetInDestBuffer, numSamples);
                DBG ("*** Cache miss");
                break;
            }
        }

        lastReadTime = juce::Time::getApproximateMillisecondCounter();
        return allDataRead;
    }

    bool getRange (juce::int64 startSample, int numSamples,
                   float& lmax, float& lmin, float& rmax, float& rmin,
                   const int timeoutMs)
    {
        jassert (startSample >= 0);
        bool allDataRead = true, isFirst = true;

        while (numSamples > 0)
        {
            const LockedReaderFinder l (*this, startSample, timeoutMs);

            if (l.isLocked && l.reader != nullptr)
            {
                auto numThisTime = std::min (numSamples, (int) (l.reader->getMappedSection().getEnd() - startSample));

                if (isFirst)
                {
                    isFirst = false;
                    l.reader->readMaxLevels (startSample, numThisTime, lmin, lmax, rmin, rmax);
                }
                else
                {
                    float lmin2, lmax2, rmin2, rmax2;
                    l.reader->readMaxLevels (startSample, numThisTime, lmin2, lmax2, rmin2, rmax2);

                    lmin = std::min (lmin, lmin2);
                    lmax = std::max (lmax, lmax2);
                    rmin = std::min (rmin, rmin2);
                    rmax = std::max (rmax, rmax2);
                }

                startSample += numThisTime;
                numSamples -= numThisTime;
            }
            else
            {
                allDataRead = false;

                if (isFirst)
                    lmin = lmax = rmin = rmax = 0;

                break;
            }
        }

        lastReadTime = juce::Time::getApproximateMillisecondCounter();
        return allDataRead;
    }

    void addClient (Reader* r)
    {
        juce::ScopedWriteLock sl (clientListLock);
        clients.add (r);
    }

    AudioFileCache& cache;
    AudioFile file;
    AudioFileInfo info;

    std::atomic<juce::uint32> lastReadTime { juce::Time::getApproximateMillisecondCounter() };
    juce::int64 totalBytesInUse = 0;

private:
    juce::OwnedArray<juce::MemoryMappedAudioFormatReader> readers;
    juce::ReferenceCountedArray<Reader> clients;

    juce::CriticalSection blockUpdateLock;
    juce::Array<int> currentBlocks;

    bool mapEntireFile = false;
    bool failedToOpenFile = false;
    juce::uint32 lastFailedOpenAttempt = 0;

    juce::ReadWriteLock clientListLock, readerLock;

    juce::MemoryMappedAudioFormatReader* findReaderFor (juce::int64 sample) const
    {
        for (auto r : readers)
            if (r != nullptr && r->getMappedSection().contains (sample))
                return r;

        return {};
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CachedFile)
};

//==============================================================================
class AudioFileCache::MapperThread   : public juce::Thread
{
public:
    MapperThread (AudioFileCache& c)
       : juce::Thread ("CacheMapper"), owner (c)
    {
    }

    ~MapperThread()
    {
        stopThread (15000);
    }

    void run()
    {
        juce::FloatVectorOperations::disableDenormalisedNumberSupport();

        juce::uint32 lastOldFlePurge = 0;

        while (! threadShouldExit())
        {
            if (owner.serviceNextReader())
                continue;

            auto now = juce::Time::getApproximateMillisecondCounter();

            if (now > lastOldFlePurge + 2000)
            {
                lastOldFlePurge = now;
                owner.purgeOldFiles();
                continue;
            }

            wait (20);

            //DBG ("Total cache mapping: " << owner.getBytesInUse() / (1024 * 1024) << " Mb");
        }
    }

    AudioFileCache& owner;
};

//==============================================================================
class AudioFileCache::RefresherThread   : public juce::Thread
{
public:
    RefresherThread (AudioFileCache& c)  : juce::Thread ("CacheRefresher"), owner (c)
    {
    }

    ~RefresherThread()
    {
        stopThread (15000);
    }

    void run()
    {
        juce::FloatVectorOperations::disableDenormalisedNumberSupport();

        while (! threadShouldExit())
        {
            {
                const ScopedCpuMeter cpu (owner.cpuUsage, 0.2);
                owner.touchReaders();
            }

            wait (TransportControl::getNumPlayingTransports (owner.engine) > 0 ? 10 : 250);
        }
    }

    AudioFileCache& owner;
};

//==============================================================================
AudioFileCache::AudioFileCache (Engine& e)  : engine (e)
{
    CRASH_TRACER
    const int defaultSize = 6 * 48000;

    // TODO: when we drop 32-bit support, delete the cache size and related code
    setCacheSizeSamples (engine.getPropertyStorage().getProperty (SettingID::cacheSizeSamples, defaultSize));
}

AudioFileCache::~AudioFileCache()
{
    CRASH_TRACER
    stopThreads();
    purgeOrphanReaders();
    jassert (activeFiles.isEmpty());
    activeFiles.clear();
}

//==============================================================================
void AudioFileCache::stopThreads()
{
    CRASH_TRACER

    if (mapperThread != nullptr)
        mapperThread->signalThreadShouldExit();

    if (refresherThread != nullptr)
        refresherThread->signalThreadShouldExit();

    mapperThread.reset();
    refresherThread.reset();
}

void AudioFileCache::setCacheSizeSamples (juce::int64 samples)
{
    CRASH_TRACER
    samples = juce::jlimit ((juce::int64) 48000, (juce::int64) 48000 * 60, samples);

    if (cacheSizeSamples != samples)
    {
        stopThreads();

        cacheSizeSamples = samples;
        engine.getPropertyStorage().setProperty (SettingID::cacheSizeSamples, (int) cacheSizeSamples);

        {
            const juce::ScopedWriteLock sl (fileListLock);

            purgeOrphanReaders();
            releaseAllFiles();
        }

        mapperThread.reset (new MapperThread (*this));
        mapperThread->startThread (5);

        refresherThread.reset (new RefresherThread (*this));
        refresherThread->startThread (6);
    }
}

//==============================================================================
AudioFileCache::CachedFile* AudioFileCache::getOrCreateCachedFile (const AudioFile& f)
{
    for (auto s : activeFiles)
        if (s->info.hashCode == f.getHash())
            return s;

    auto& manager = engine.getAudioFileFormatManager().memoryMappedFormatManager;

    for (auto af : manager)
    {
        if (af->canHandleFile (f.getFile()))
        {
            auto fs = new CachedFile (*this, f);
            activeFiles.add (fs);
            return fs;
        }
    }

    return {};
}

void AudioFileCache::releaseFile (const AudioFile& file)
{
    const juce::ScopedReadLock sl (fileListLock);

    for (auto f : activeFiles)
        if (f->file == file)
            f->releaseReader();
}

void AudioFileCache::releaseAllFiles()
{
    CRASH_TRACER
    const juce::ScopedReadLock sl (fileListLock);

    for (auto f : activeFiles)
        f->releaseReader();
}

void AudioFileCache::validateFile (const AudioFile& file)
{
    const juce::ScopedReadLock sl (fileListLock);

    for (auto f : activeFiles)
        if (f->file == file)
            f->validateFile();
}

void AudioFileCache::purgeOldFiles()
{
    CRASH_TRACER
    auto oldestAllowedTime = juce::Time::getApproximateMillisecondCounter() - 2000;

    const juce::ScopedWriteLock sl (fileListLock);

    for (auto f :  activeFiles)
        f->purgeOrphanReaders();

    for (int i = activeFiles.size(); --i >= 0;)
    {
        auto f = activeFiles.getUnchecked (i);

        if (f->lastReadTime < oldestAllowedTime && f->isUnused())
            activeFiles.remove (i);
    }
}

bool AudioFileCache::serviceNextReader()
{
    const juce::ScopedReadLock sl (fileListLock);

    for (int i = activeFiles.size(); --i >= 0;)
    {
        if (++nextFileToService >= activeFiles.size())
            nextFileToService = 0;

        auto* f = activeFiles.getUnchecked (nextFileToService);

        if (f->updateBlocks())
            return true;
    }

    return false;
}

void AudioFileCache::touchReaders()
{
    juce::int64 totalBytes = 0;

    const juce::ScopedReadLock sl (fileListLock);

    for (auto f : activeFiles)
    {
        f->touchFiles();
        totalBytes += f->totalBytesInUse;
    }

    totalBytesUsed = totalBytes;
}

bool AudioFileCache::hasCacheMissed (bool clearMissedFlag)
{
    const bool didMiss = cacheMissed;

    if (clearMissedFlag)
        cacheMissed = false;

    return didMiss;
}

//==============================================================================
AudioFileCache::Reader::Ptr AudioFileCache::createReader (const AudioFile& file)
{
    CRASH_TRACER
    const juce::ScopedWriteLock sl (fileListLock);

    if (auto f = getOrCreateCachedFile (file))
    {
        auto r = new Reader (*this, f, nullptr);
        f->addClient (r);
        return r;
    }

    if (auto reader = AudioFileUtils::createReaderFor (file.getFile()))
    {
        backgroundReaderThread.startThread (4);

        return new Reader (*this, nullptr, new juce::BufferingAudioReader (reader, backgroundReaderThread,
                                                                           48000 * 5));
    }

    return {};
}

void AudioFileCache::purgeOrphanReaders()
{
    for (CachedFile* f : activeFiles)
        f->purgeOrphanReaders();

    for (int i = activeFiles.size(); --i >= 0;)
        if (activeFiles.getUnchecked(i)->isUnused())
            activeFiles.remove (i);
}

//==============================================================================
AudioFileCache::Reader::Reader (AudioFileCache& c, void* f, juce::BufferingAudioReader* fallback)
    : cache (c), file (f), fallbackReader (fallback)
{
    jassert (file != nullptr || fallbackReader != nullptr);
}

AudioFileCache::Reader::~Reader()
{
}

void AudioFileCache::Reader::setReadPosition (juce::int64 pos) noexcept
{
    const auto localLoopStart = loopStart.load();
    const auto localLoopLength = loopLength.load();

    if (loopLength == 0)
        readPos = pos;
    else if (pos >= 0)
        readPos = localLoopStart + (pos % localLoopLength);
    else
        readPos = localLoopStart + juce::negativeAwareModulo (pos, localLoopLength);
}

int AudioFileCache::Reader::getNumChannels() const noexcept
{
    return file != nullptr ? static_cast<CachedFile*> (file)->info.numChannels
                           : (int) fallbackReader->numChannels;
}

double AudioFileCache::Reader::getSampleRate() const noexcept
{
    return file != nullptr ? static_cast<CachedFile*> (file)->info.sampleRate
                           : (int) fallbackReader->sampleRate;
}

void AudioFileCache::Reader::setLoopRange (juce::Range<juce::int64> newRange)
{
    loopStart  = newRange.getStart();
    loopLength = newRange.getLength();
}

bool AudioFileCache::Reader::readSamples (int numSamples,
                                          juce::AudioBuffer<float>& destBuffer,
                                          const juce::AudioChannelSet& destBufferChannels,
                                          int startOffsetInDestBuffer,
                                          const juce::AudioChannelSet& sourceBufferChannels,
                                          int timeoutMs)
{
    jassert (numSamples < CachedFile::readAheadSamples); // this method fails unless broken down into chunks smaller than this
    const auto numDestChans = destBuffer.getNumChannels();

    // This may need to deal with the generic surround case if destBuffer number of channels > channelsToUse.size()
    if (cache.engine.getEngineBehaviour().isDescriptionOfWaveDevicesSupported())
    {
        static constexpr int maxNumChannels = 32;
        float* chans[maxNumChannels] = {};
        auto numSourceChans = std::min (maxNumChannels, sourceBufferChannels.size());
        int highestUsedSourceChan = 0;

        for (int destIndex = 0; destIndex < numDestChans; ++destIndex)
        {
            auto destType = destBufferChannels.getTypeOfChannel (destIndex);
            auto destData = destBuffer.getWritePointer (destIndex, startOffsetInDestBuffer);
            auto sourceIndex = sourceBufferChannels.getChannelIndexForType (destType);

            if (sourceIndex >= 0 && sourceIndex < maxNumChannels)
            {
                chans[sourceIndex] = destData;
                highestUsedSourceChan = std::max (highestUsedSourceChan, sourceIndex);
            }
            else
            {
                juce::FloatVectorOperations::clear (destData, numSamples);
            }
        }

        if (readSamples ((int**) chans, numSourceChans, 0, numSamples, timeoutMs))
        {
            bool isFloatingPoint = (file != nullptr) ? static_cast<CachedFile*> (file)->info.isFloatingPoint
                                                     : fallbackReader->usesFloatingPointData;

            if (! isFloatingPoint)
                for (int i = 0; i <= highestUsedSourceChan; ++i)
                    if (auto chan = chans[i])
                        juce::FloatVectorOperations::convertFixedToFloat (chan, (const int*) chan, 1.0f / 0x7fffffff, numSamples);

            return true;
        }
    }
    else
    {
        float* chans[2] = {};
        bool dupeChannel = false;

        if (numDestChans > 1)
        {
            if (sourceBufferChannels.getChannelIndexForType (juce::AudioChannelSet::left) >= 0
                 && sourceBufferChannels.getChannelIndexForType (juce::AudioChannelSet::right) >= 0)
            {
                chans[0] = destBuffer.getWritePointer (0, startOffsetInDestBuffer);

                if (getNumChannels() > 1)
                    chans[1] = destBuffer.getWritePointer (1, startOffsetInDestBuffer);
                else
                    dupeChannel = true;
            }
            else if (sourceBufferChannels.getChannelIndexForType (juce::AudioChannelSet::left) >= 0)
            {
                chans[0] = destBuffer.getWritePointer (0, startOffsetInDestBuffer);
                dupeChannel = true;
            }
            else
            {
                chans[1] = destBuffer.getWritePointer (1, startOffsetInDestBuffer);
                dupeChannel = true;
            }
        }
        else
        {
            if (sourceBufferChannels.getChannelIndexForType (juce::AudioChannelSet::left) >= 0 || getNumChannels() < 2)
                chans[0] = destBuffer.getWritePointer (0, startOffsetInDestBuffer);
            else
                chans[1] = destBuffer.getWritePointer (0, startOffsetInDestBuffer);
        }

        if (readSamples ((int**) chans, 2, 0, numSamples, timeoutMs))
        {
            const bool isFloatingPoint = (file != nullptr) ? static_cast<CachedFile*> (file)->info.isFloatingPoint
                                                           : fallbackReader->usesFloatingPointData;

            if (! isFloatingPoint)
                for (int i = 0; i < 2; ++i)
                    if (auto* chan = chans[i])
                        juce::FloatVectorOperations::convertFixedToFloat (chan, (const int*) chan, 1.0f / 0x7fffffff, numSamples);

            if (dupeChannel)
            {
                if (chans[0] == nullptr)
                    juce::FloatVectorOperations::copy (destBuffer.getWritePointer (0), chans[1], numSamples);
                else if (chans[1] == nullptr)
                    juce::FloatVectorOperations::copy (destBuffer.getWritePointer (1), chans[0], numSamples);
            }

            return true;
        }
    }

    return false;
}

bool AudioFileCache::Reader::readSamples (int** destSamples, int numDestChannels,
                                          int startOffsetInDestBuffer, int numSamples, int timeoutMs)
{
    jassert (numSamples < CachedFile::readAheadSamples); // this method fails unless broken down into chunks smaller than this
    jassert (getReferenceCount() > 1 || file == nullptr); // may be being used after the cache has been deleted
    jassert (timeoutMs >= 0);

    if (readPos < 0)
    {
        auto silence = (int) std::min (-readPos, (juce::int64) numSamples);

        for (int i = numDestChannels; --i >= 0;)
            if (destSamples[i] != nullptr)
                juce::FloatVectorOperations::clear ((float*) destSamples[i], silence);

        startOffsetInDestBuffer += silence;
        numSamples -= silence;
        readPos += silence;

        if (numSamples <= 0)
            return true;
    }

    bool allOk = true;

    if (loopLength == 0)
    {
        if (auto cf = static_cast<CachedFile*> (file))
        {
            allOk = cf->read (readPos, destSamples, numDestChannels, startOffsetInDestBuffer, numSamples, timeoutMs);
        }
        else
        {
            fallbackReader->setReadTimeout (timeoutMs);
            allOk = fallbackReader->readSamples (destSamples, numDestChannels, startOffsetInDestBuffer, readPos, numSamples);
        }

        readPos += numSamples;
    }
    else if (loopLength > 1)
    {
        while (numSamples > 0)
        {
            jassert (juce::isPositiveAndBelow (readPos.load() - loopStart.load(), loopLength.load()));

            auto numToRead = (int) std::min ((juce::int64) numSamples, loopStart + loopLength - readPos);

            if (auto cf = static_cast<CachedFile*> (file))
            {
                allOk = cf->read (readPos, destSamples, numDestChannels, startOffsetInDestBuffer, numToRead, timeoutMs) && allOk;
            }
            else
            {
                fallbackReader->setReadTimeout (timeoutMs);
                allOk = fallbackReader->readSamples (destSamples, numDestChannels, startOffsetInDestBuffer, readPos, numToRead) && allOk;
            }

            readPos += numToRead;

            if (readPos >= loopStart + loopLength)
                readPos -= loopLength;

            startOffsetInDestBuffer += numToRead;
            numSamples -= numToRead;
        }

        return allOk;
    }
    else
    {
        clearSetOfChannels (destSamples, numDestChannels, startOffsetInDestBuffer, numSamples);
    }

    if (! allOk)
        cache.cacheMissed = true;

    return allOk;
}

bool AudioFileCache::Reader::getRange (int numSamples, float& lmax, float& lmin, float& rmax, float& rmin, int timeoutMs)
{
    jassert (getReferenceCount() > 1 || file == nullptr); // may be being used after the cache has been deleted

    bool ok;

    if (auto cf = static_cast<CachedFile*> (file))
    {
        ok = cf->getRange (readPos, numSamples, lmax, lmin, rmax, rmin, timeoutMs);
    }
    else
    {
        fallbackReader->setReadTimeout (timeoutMs);
        fallbackReader->readMaxLevels (readPos, numSamples, lmin, lmax, rmin, rmax);
        ok = true;
    }

    readPos += numSamples;
    return ok;
}

//==============================================================================
struct CacheAudioFormatReader  :  public juce::AudioFormatReader
{
    CacheAudioFormatReader (const AudioFile& file)
        : juce::AudioFormatReader (nullptr, "Cache")
    {
        auto info = file.getInfo();

        sampleRate = info.sampleRate;
        bitsPerSample = (unsigned int) info.bitsPerSample;
        lengthInSamples = info.lengthInSamples;
        numChannels = (unsigned int) info.numChannels;
        usesFloatingPointData = info.isFloatingPoint;
        metadataValues = info.metadata;

        reader = Engine::getInstance().getAudioFileManager().cache.createReader (file);
    }

    void readMaxLevels (juce::int64 startSample, juce::int64 numSamples,
                        float& lowestLeft, float& highestLeft,
                        float& lowestRight, float& highestRight) override
    {
        reader->setReadPosition (startSample);
        reader->getRange ((int) numSamples, highestLeft, lowestLeft, highestRight, lowestRight, -1);
    }

    bool readSamples (int** destSamples, int numDestChannels,
                      int startOffsetInDestBuffer, juce::int64 startSampleInFile,
                      int numSamples) override
    {
        reader->setReadPosition (startSampleInFile);
        return reader->readSamples (destSamples, numDestChannels, startOffsetInDestBuffer, numSamples, -1);
    }

private:
    AudioFileCache::Reader::Ptr reader;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CacheAudioFormatReader)
};
