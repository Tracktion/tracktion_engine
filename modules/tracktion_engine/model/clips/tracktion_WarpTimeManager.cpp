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

inline HashCode hashDouble (double d) noexcept
{
    static_assert (sizeof (double) == sizeof (int64_t), "double and int64 different sizes");
    union Val { double asDouble; int64_t asInt; } v;
    v.asDouble = d;
    return v.asInt;
}

HashCode WarpMarker::getHash() const noexcept    { return hashDouble (sourceTime.inSeconds()) ^ hashDouble (warpTime.inSeconds()); }

template <typename FloatingPointType>
struct Differentiator
{
    void process (const FloatingPointType* inputSamples, int numSamples, FloatingPointType* outputSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            FloatingPointType currentSample = inputSamples[i];
            outputSamples[i] = currentSample - lastSample;
            lastSample = currentSample;
        }
    }

    FloatingPointType lastSample { (FloatingPointType) 0.0 };
};

//==============================================================================
struct TransientDetectionJob  : public RenderManager::Job
{
    struct Config
    {
        float sensitivity;
    };

    static Ptr getOrCreateDetectionJob (Engine& e, const AudioFile& file, Config config)
    {
        auto jobs = e.getRenderManager().getRenderJobsWithoutCreating (file);

        for (auto j : jobs)
            if (auto tdj = dynamic_cast<TransientDetectionJob*> (j))
                if (tdj->isIdenticalTo (file, config))
                    return j;

        return new TransientDetectionJob (e, file, config);
    }

    bool isIdenticalTo (const AudioFile& f, Config c)
    {
        return file == f && config.sensitivity == c.sensitivity;
    }

    juce::Array<TimePosition> getTimes() const      { return transientTimes; }

protected:
    bool setUpRender() override        { return reader != nullptr && totalNumSamples > 0; }

    bool completeRender() override
    {
        if (transientTimes.size() > 1)
        {
            auto trimTransients = [this]() -> bool
            {
                const auto minTime = 0.1s;
                auto lastTime = transientTimes.getLast();
                const int initialSize = transientTimes.size();

                for (int i = transientTimes.size() - 1; --i >= 0;)
                {
                    const auto t = transientTimes.getUnchecked (i);

                    if ((lastTime - t) < minTime)
                        transientTimes.remove (i);
                    else
                        lastTime = t;
                }

                return initialSize != transientTimes.size();
            };

            for (int i = 10; --i >= 0;)
                if (! trimTransients())
                    break;
        }

        return true;
    }

    bool renderNextBlock() override
    {
        CRASH_TRACER
        auto numLeft = totalNumSamples - numSamplesRead;
        auto numToDo = (int) std::min ((SampleCount) 32768, numLeft);

        AudioScratchBuffer scratch (numChannels, numToDo);
        auto schannels = juce::AudioChannelSet::canonicalChannelSet(numChannels);
        reader->readSamples (numToDo, scratch.buffer, schannels, 0, juce::AudioChannelSet::stereo(), 5000);

        if (findingNormaliseLevel)
            processNextNormaliseBuffer (scratch.buffer);
        else
            processNextBuffer (scratch.buffer);

        numSamplesRead += numToDo;
        progress = (numSamplesRead / (float) totalNumSamples)
            * (findingNormaliseLevel ? 0.5f : 1.0f);

        if (findingNormaliseLevel && numSamplesRead >= totalNumSamples)
        {
            auto peak = std::max (std::abs (fileMinMax.getStart()),
                                  std::abs (fileMinMax.getEnd()));
            normaliseScale = peak > 0.0f ? 1.0f / peak : 1.0f;
            reader->setReadPosition (0);
            numSamplesRead = 0;
            findingNormaliseLevel = false;
        }

        return ! findingNormaliseLevel && numSamplesRead >= totalNumSamples;
    }

private:
    AudioFile file;
    Config config;

    SampleCount numSamplesRead = 0, totalNumSamples = 0;
    int numChannels = 0;
    double sampleRate = 0;
    AudioFileCache::Reader::Ptr reader;

    AudioFileUtils::EnvelopeFollower envelopeFollower[3];
    Differentiator<float> differentiator;
    juce::Array<TimePosition> transientTimes;

    juce::Range<float> fileMinMax;
    float normaliseScale = -1.0f;
    const float thresh = juce::Decibels::decibelsToGain (-25.0f);
    int triggerTimer = 0;
    int countDownTimer = 0;
    bool findingNormaliseLevel = true;

    TransientDetectionJob (Engine& e, const AudioFile& af, Config c)
        : Job (e, AudioFile (e)), file (af), config (c),
          totalNumSamples (af.getLengthInSamples()),
          numChannels (af.getNumChannels()),
          reader (e.getAudioFileManager().cache.createReader (af))
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        // N.B. The argumnet to the Job constructor is the proxy file to use
        // Don't send the audio file here or it will get deleted!
        jassert (proxy.isNull());

        if (reader != nullptr)
            sampleRate = reader->getSampleRate();

        triggerTimer = int (sampleRate * 50 * 0.001); // 50ms - should be linked to BPM
        envelopeFollower[0].setCoefficients (1.0, 0.002f);
        envelopeFollower[1].setCoefficients (1.0, 0.002f);
        envelopeFollower[2].setCoefficients (1.0, 0.002f);
    }

    void processNextNormaliseBuffer (const juce::AudioBuffer<float>& buffer)
    {
        fileMinMax = fileMinMax.getUnionWith (juce::FloatVectorOperations::findMinAndMax (buffer.getReadPointer (0),
                                                                                          buffer.getNumSamples()));
    }

    void processNextBuffer (const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        AudioScratchBuffer scratch (1, numSamples);
        auto& detectionBuffer = scratch.buffer;

        detectionBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
        detectionBuffer.applyGain (0, 0, numSamples, normaliseScale);
        float* data = detectionBuffer.getWritePointer (0);

        envelopeFollower[0].processEnvelope (data, data, numSamples);
        envelopeFollower[1].processEnvelope (data, data, numSamples);

        differentiator.process (data, numSamples, data);
        envelopeFollower[2].processEnvelope (data, data, numSamples);

        for (int i = 0; i < numSamples; i++)
        {
            const float sample = *data++;

            if (countDownTimer)
                countDownTimer--;

            if (sample > thresh)
            {
                if (countDownTimer == 0)
                {
                    int rewindIndex = int (i - sampleRate * 0.0005); //rewind by 0.05ms

                    if (rewindIndex < 0)
                        rewindIndex = 0;

                    transientTimes.add (sampleToSeconds (numSamplesRead + rewindIndex));
                }

                countDownTimer = triggerTimer;
            }
        }
    }

    TimePosition sampleToSeconds (SampleCount sample) const
    {
        return TimePosition::fromSeconds (sampleRate > 0.0 ? sample / sampleRate : 0.0);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientDetectionJob)
};

//==============================================================================
WarpTimeManager::WarpTimeManager (AudioClipBase& c)
    : edit (c.edit), clip (&c), sourceFile (c.edit.engine)
{
    state = c.state.getOrCreateChildWithName (IDs::WARPTIME, &edit.getUndoManager());
    auto markersTree = state.getOrCreateChildWithName (IDs::WARPMARKERS, &edit.getUndoManager());
    markers.reset (new WarpMarkerList (markersTree));

    const auto clipLen = toPosition (TimeDuration::fromSeconds (AudioFile (c.edit.engine, clip->getOriginalFile()).getLength()));

    // If this is the first time that we've built the Manager
    if (markers->isEmpty())
    {
        insertMarker (WarpMarker());
        insertMarker (WarpMarker (clipLen, clipLen));
        setWarpEndMarkerTime (clipLen);
    }

    editLoadedCallback.reset (new Edit::LoadFinishedCallback<WarpTimeManager> (*this, edit));

    edit.engine.getWarpTimeFactory().addWarpTimeManager (*this);
}

WarpTimeManager::WarpTimeManager (Edit& e, const AudioFile& f, juce::ValueTree parentTree)
    : edit (e), sourceFile (f), endMarkerEnabled (false), endMarkersLimited (true)
{
    state = parentTree.getOrCreateChildWithName (IDs::WARPTIME, &edit.getUndoManager());
    auto markersTree = state.getOrCreateChildWithName (IDs::WARPMARKERS, &edit.getUndoManager());
    markers.reset (new WarpMarkerList (markersTree));

    setSourceFile (f);

    edit.engine.getWarpTimeFactory().addWarpTimeManager (*this);
}

WarpTimeManager::~WarpTimeManager()
{
    if (transientDetectionJob != nullptr)
        transientDetectionJob->removeListener (this);

    transientDetectionJob = nullptr;
    edit.engine.getWarpTimeFactory().removeWarpTimeManager (*this);
}

void WarpTimeManager::setSourceFile (const AudioFile& af)
{
    jassert (clip == nullptr);
    sourceFile = af;

    if (markers->isEmpty())
    {
        const auto clipLen = toPosition (TimeDuration::fromSeconds (sourceFile.getLength()));

        if (sourceFile.isValid())
        {
            insertMarker (WarpMarker());
            insertMarker (WarpMarker (clipLen, clipLen));
            setWarpEndMarkerTime (clipLen);

            editLoadedCallback.reset (new Edit::LoadFinishedCallback<WarpTimeManager> (*this, edit));
        }
    }
}

AudioFile WarpTimeManager::getSourceFile() const
{
    return clip != nullptr ? AudioFile (clip->edit.engine, clip->getOriginalFile()) : sourceFile;
}

TimeDuration WarpTimeManager::getSourceLength() const
{
    return TimeDuration::fromSeconds (getSourceFile().getLength());
}

int WarpTimeManager::insertMarker (WarpMarker marker)
{
    int index = 0;

    while (index < markers->objects.size() && markers->objects.getUnchecked (index)->warpTime < marker.warpTime)
        index++;

    auto v = createValueTree (IDs::WARPMARKER,
                              IDs::sourceTime, marker.sourceTime.inSeconds(),
                              IDs::warpTime, marker.warpTime.inSeconds());

    markers->state.addChild (v, index, getUndoManager());

    return index;
}

void WarpTimeManager::removeMarker (int index)
{
    if (index == 0 || index == markers->size() - 1)
        moveMarker (index, markers->objects.getUnchecked (index)->sourceTime);
    else
        markers->state.removeChild (index, getUndoManager());
}

void WarpTimeManager::removeAllMarkers()
{
    markers->state.removeAllChildren (getUndoManager());
    const auto clipLen = toPosition (getSourceLength());

    insertMarker (WarpMarker());
    insertMarker (WarpMarker (clipLen, clipLen));
    setWarpEndMarkerTime (clipLen);
}

TimePosition WarpTimeManager::moveMarker (int index, TimePosition newWarpTime)
{
    CRASH_TRACER
    auto m = markers->state.getChild (index);

    if (! m.isValid())
        return newWarpTime;

    if (index > 0)
    {
        WarpMarker* a = markers->objects.getUnchecked (index - 1);
        WarpMarker* b = markers->objects.getUnchecked (index);
        auto srcLen = b->sourceTime - a->sourceTime;
        double stretchRatio = (newWarpTime - a->warpTime) / srcLen;

        if (stretchRatio < 0.10001)
            newWarpTime = a->warpTime + srcLen * 0.10001;
        else if (stretchRatio > 19.9999)
            newWarpTime = a->warpTime + srcLen * 19.9999;
    }

    if (index < markers->objects.size() - 1)
    {
        WarpMarker* a = markers->objects.getUnchecked (index);
        WarpMarker* b = markers->objects.getUnchecked (index + 1);
        auto srcLen = b->sourceTime - a->sourceTime;
        double stretchRatio = (b->warpTime - newWarpTime) / srcLen;

        if (stretchRatio < 0.10001)
            newWarpTime = b->warpTime - srcLen * 0.10001;
        else if (stretchRatio > 19.9999)
            newWarpTime = b->warpTime - srcLen * 19.9999;
    }

    if (endMarkersLimited && (index == 0 || (index == markers->objects.size() - 1)))
        newWarpTime = juce::jlimit (TimePosition(), toPosition (getSourceLength()), newWarpTime);

    m.setProperty (IDs::warpTime, newWarpTime.inSeconds(), getUndoManager());

    return newWarpTime;
}

void WarpTimeManager::setWarpEndMarkerTime (TimePosition endTime)
{
    if (endTime > 0.0s)
        state.setProperty (IDs::warpEndMarkerTime, endTime.inSeconds(), &edit.getUndoManager());
}

juce::Array<TimeRange> WarpTimeManager::getWarpTimeRegions (const TimeRange overallTimeRegion) const
{
    juce::Array<TimeRange> visibleWarpRegions;
    auto& markersArray = markers->objects;

    if (markersArray.isEmpty())
    {
        visibleWarpRegions.add (overallTimeRegion);
        return visibleWarpRegions;
    }

    auto timeRegion = overallTimeRegion;
    TimeDuration overallTime;
    auto warpedClipLength = getWarpedEnd();

    // trim this region to the end of the clip content.
    if (timeRegion.getEnd() > warpedClipLength)
        timeRegion = timeRegion.withEnd (warpedClipLength);

    //set up the warp regions
    TimeRange warpRegion (overallTimeRegion.getStart(), warpedClipLength);

    for (int markerIndex = 0; markerIndex <= markersArray.size(); markerIndex++)
    {
        if (markerIndex == markersArray.size()) // if we're on the last region
            warpRegion = warpRegion.withEnd (std::max (warpRegion.getStart(), warpedClipLength));
        else
            warpRegion = warpRegion.withEnd (std::max (warpRegion.getStart(), markersArray.getUnchecked (markerIndex)->warpTime));

        auto warpRegionConstrained = timeRegion.getIntersectionWith (warpRegion);

        if (warpRegionConstrained.getLength() > 0s) // don't add zero length regions
        {
            visibleWarpRegions.add (warpRegionConstrained);
            overallTime = overallTime + warpRegionConstrained.getLength();
        }

        warpRegion = warpRegion.withStart (warpRegion.getEnd());
    }

    return visibleWarpRegions;
}

TimePosition WarpTimeManager::warpTimeToSourceTime (TimePosition warpTime) const
{
    auto& markersArray = markers->objects;

    if (markersArray.isEmpty())
        return warpTime;

    WarpMarker startMarker, endMarker;

    auto first = *markersArray.getFirst();
    auto last  = *markersArray.getLast();

    if (warpTime <= first.warpTime) //below or on the 1st marker
    {
        startMarker = {};
        endMarker = first;
    }
    else if (warpTime > last.warpTime) // after the last marker
    {
        startMarker = last;
        auto sourceLen = toPosition (clip->getSourceLength());
        endMarker = WarpMarker (sourceLen, sourceLen);
    }
    else
    {
        int index = 0;
        auto numMarkers = markersArray.size();

        while (index < numMarkers && markersArray.getUnchecked (index)->warpTime < warpTime)
            index++;

        if (index > 0)
            startMarker = *markersArray.getUnchecked (index - 1);

        endMarker = *markersArray.getUnchecked (index);
    }

    const WarpMarker markerRanges (toPosition (endMarker.sourceTime - startMarker.sourceTime),
                                   toPosition (endMarker.warpTime - startMarker.warpTime));

    TimePosition sourcePosition;

    if (markerRanges.warpTime == 0.0s)
    {
        sourcePosition = 0.0s;
    }
    else
    {
        const double warpProportion = (warpTime - startMarker.warpTime) / toDuration (markerRanges.warpTime);
        sourcePosition = (markerRanges.sourceTime * warpProportion) + toDuration (startMarker.sourceTime);
    }

    return sourcePosition;
}

TimePosition WarpTimeManager::sourceTimeToWarpTime (TimePosition sourceTime) const
{
    auto& markersArray = markers->objects;

    if (markersArray.isEmpty())
        return sourceTime;

    WarpMarker* before = nullptr;
    WarpMarker* after = nullptr;

    for (auto wm : markersArray)
    {
        before = after;
        after = wm;

        if (before != nullptr && after != nullptr
            && (sourceTime >= before->sourceTime && sourceTime <= after->sourceTime))
                break;
    }

    TimeRange source (before == nullptr ? TimePosition() : before->sourceTime,
                      after == nullptr ? toPosition (getSourceLength()) : after->sourceTime);

    if (source.getLength() == 0.0s)
        return sourceTime;

    auto prop = (sourceTime - source.getStart()) / source.getLength();

    TimeRange warped (before == nullptr ? TimePosition() : before->warpTime,
                          after == nullptr ? getWarpedEnd() : after->warpTime);

    return warped.getStart() + (warped.getLength() * prop);
}

TimePosition WarpTimeManager::getWarpedStart() const
{
    jassert (markers->size() != 0);

    return markers->objects.getFirst()->warpTime;
}

TimePosition WarpTimeManager::getWarpedEnd() const
{
    jassert (markers->size() != 0);

    return markers->objects.getLast()->warpTime;
}

HashCode WarpTimeManager::getHash() const
{
    HashCode h = 0;

    for (auto wm : markers->objects)
        h ^= wm->getHash();

    h ^= hashDouble (getWarpEndMarkerTime().inSeconds());

    return h;
}

TimePosition WarpTimeManager::getWarpEndMarkerTime() const
{
    if (isWarpEndMarkerEnabled())
        return TimePosition::fromSeconds (state.getProperty (IDs::warpEndMarkerTime, 0.0));

    return toPosition (getSourceLength());
}

void WarpTimeManager::editFinishedLoading()
{
    TransientDetectionJob::Config config;
    config.sensitivity = 0.5f;
    transientDetectionJob = TransientDetectionJob::getOrCreateDetectionJob (edit.engine, getSourceFile(), config);

    if (transientDetectionJob != nullptr)
        transientDetectionJob->addListener (this);

    editLoadedCallback = nullptr;
}

juce::UndoManager* WarpTimeManager::getUndoManager() const
{
    return &edit.getUndoManager();
}

void WarpTimeManager::jobFinished (RenderManager::Job& job, bool /*completedOk*/)
{
    if (auto tdj = dynamic_cast<TransientDetectionJob*> (&job))
    {
        transientTimes.second = tdj->getTimes();
        transientTimes.first = true;
    }

    job.removeListener (this);
    transientDetectionJob = nullptr;
}

//==============================================================================
WarpTimeManager::Ptr WarpTimeFactory::getWarpTimeManager (const Clip& clip)
{
    {
        const juce::ScopedLock sl (warpTimeLock);

        for (auto c : warpTimeManagers)
            if (c->clip == &clip)
                return c;
    }

    if (auto wac = const_cast<WaveAudioClip*> (dynamic_cast<const WaveAudioClip*> (&clip)))
        return new WarpTimeManager (*wac);

    jassertfalse;
    return {};
}

void WarpTimeFactory::addWarpTimeManager (WarpTimeManager& wtm)
{
    const juce::ScopedLock sl (warpTimeLock);
    jassert (! warpTimeManagers.contains (&wtm));
    warpTimeManagers.addIfNotAlreadyThere (&wtm);
}

void WarpTimeFactory::removeWarpTimeManager (WarpTimeManager& wtm)
{
    const juce::ScopedLock sl (warpTimeLock);
    jassert (warpTimeManagers.contains (&wtm));
    warpTimeManagers.removeAllInstancesOf (&wtm);
}

}} // namespace tracktion { inline namespace engine
