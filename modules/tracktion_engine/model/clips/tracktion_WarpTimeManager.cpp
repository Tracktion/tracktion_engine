/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


inline int64 hashDouble (double d) noexcept
{
    static_assert (sizeof (double) == sizeof (int64), "double and int64 different sizes");
    union Val { double asDouble; int64 asInt; } v;
    v.asDouble = d;

    return v.asInt;
}

int64 WarpMarker::getHash() const noexcept    { return hashDouble (sourceTime) ^ hashDouble (warpTime); }

//==============================================================================
template <typename FloatingPointType>
class Differentiator
{
public:
    void process (const FloatingPointType* inputSamples, int numSamples, FloatingPointType* outputSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            FloatingPointType currentSample = inputSamples[i];
            outputSamples[i] = currentSample - lastSample;
            lastSample = currentSample;
        }
    }
private:
    FloatingPointType lastSample {(FloatingPointType)0.0};
};

struct TransientDetectionJob : public RenderManager::Job
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

    Array<double> getTimes() const                  { return transientTimes; }

protected:
    bool setUpRender() override                     { return reader != nullptr && totalNumSamples > 0; }

    bool completeRender() override
    {
        if (transientTimes.size() > 1)
        {
            auto trimTransients = [this]() -> bool
            {
                const double minTime = 0.1;
                double lastTime = transientTimes.getLast();
                const int initialSize = transientTimes.size();

                for (int i = transientTimes.size() - 1; --i >= 0;)
                {
                    const double t = transientTimes.getUnchecked (i);

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
        const int64 numLeft = totalNumSamples - numSamplesRead;
        const int numToDo = (int) jmin ((int64) 32768, numLeft);

        AudioScratchBuffer scratch (numChannels, numToDo);
        AudioChannelSet schannels = AudioChannelSet::canonicalChannelSet(numChannels);
        reader->readSamples (numToDo, scratch.buffer, schannels, 0, AudioChannelSet::stereo(), 5000);

        if (findingNormaliseLevel)
            processNextNormaliseBuffer (scratch.buffer);
        else
            processNextBuffer (scratch.buffer);

        numSamplesRead += numToDo;
        progress = (numSamplesRead / (float) totalNumSamples)
            * (findingNormaliseLevel ? 0.5f : 1.0f);

        if (findingNormaliseLevel && numSamplesRead >= totalNumSamples)
        {
            const float peak = jmax (std::abs (fileMinMax.getStart()), std::abs (fileMinMax.getEnd()));
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

    int64 numSamplesRead = 0, totalNumSamples;
    int numChannels;
    double sampleRate = 0;
    AudioFileCache::Reader::Ptr reader;

    AudioFileUtils::EnvelopeFollower envelopeFollower[3];
    Differentiator<float> differentiator;
    Array<double> transientTimes;

    Range<float> fileMinMax;
    float normaliseScale = -1.0f;
    const float thresh = Decibels::decibelsToGain (-25.0f);
    int triggerTimer = 0;
    int countDownTimer = 0;
    bool findingNormaliseLevel = true;

    TransientDetectionJob (Engine& e, const AudioFile& af, Config c)
        : Job (e, {}), file (af), config (c),
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
        fileMinMax = fileMinMax.getUnionWith (FloatVectorOperations::findMinAndMax (buffer.getReadPointer (0), buffer.getNumSamples()));
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

    double sampleToSeconds (int64 sample) const
    {
        return sampleRate > 0.0 ? sample / sampleRate : 0.0;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransientDetectionJob)
};

//==============================================================================
WarpTimeManager::WarpTimeManager (AudioClipBase& c)
    : edit (c.edit), clip (&c)
{
    state = c.state.getOrCreateChildWithName (IDs::WARPTIME, &edit.getUndoManager());
    auto markersTree = state.getOrCreateChildWithName (IDs::WARPMARKERS, &edit.getUndoManager());
    markers = new WarpMarkerList (markersTree);

    const double clipLen = AudioFile (clip->getOriginalFile()).getLength();

    // If this is the first time that we've built the Manager
    if (getMarkers().isEmpty())
    {
        insertMarker (-1, WarpMarker (0, 0));
        insertMarker (-1, WarpMarker (clipLen, clipLen));
        setWarpEndMarkerTime (clipLen);
    }

    editLoadedCallback = new Edit::LoadFinishedCallback<WarpTimeManager> (*this, edit);

    edit.engine.getWarpTimeFactory().addWarpTimeManager (*this);
}

WarpTimeManager::WarpTimeManager (Edit& e, const AudioFile& f, ValueTree parentTree)
    : edit (e), sourceFile (f), endMarkerEnabled (false), endMarkersLimited (true)
{
    state = parentTree.getOrCreateChildWithName (IDs::WARPTIME, &edit.getUndoManager());
    auto markersTree = state.getOrCreateChildWithName (IDs::WARPMARKERS, &edit.getUndoManager());
    markers = new WarpMarkerList (markersTree);

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

    if (getMarkers().isEmpty())
    {
        const double clipLen = sourceFile.getLength();

        if (sourceFile.isValid())
        {
            insertMarker (-1, WarpMarker (0, 0));
            insertMarker (-1, WarpMarker (clipLen, clipLen));
            setWarpEndMarkerTime (clipLen);

            editLoadedCallback = new Edit::LoadFinishedCallback<WarpTimeManager> (*this, edit);
        }
    }
}

AudioFile WarpTimeManager::getSourceFile() const
{
    return clip != nullptr ? AudioFile (clip->getOriginalFile()) : sourceFile;
}

double WarpTimeManager::getSourceLength() const
{
    return getSourceFile().getLength();
}

void WarpTimeManager::insertMarker (int index, WarpMarker marker)
{
    ValueTree v (IDs::WARPMARKER);
    v.setProperty (IDs::sourceTime, marker.sourceTime, nullptr);
    v.setProperty (IDs::warpTime, marker.warpTime, nullptr);
    markers->state.addChild (v, index, getUndoManager());
}

void WarpTimeManager::removeMarker (int index)
{
    if (index == 0 || index == getMarkers().size() - 1)
        moveMarker (index, getMarkers().getUnchecked (index)->sourceTime);
    else
        markers->state.removeChild (index, getUndoManager());
}

void WarpTimeManager::removeAllMarkers()
{
    markers->state.removeAllChildren (getUndoManager());
    const double clipLen = getSourceLength();

    insertMarker (-1, WarpMarker (0, 0));
    insertMarker (-1, WarpMarker (clipLen, clipLen));
    setWarpEndMarkerTime (clipLen);
}

double WarpTimeManager::moveMarker (int index, double newWarpTime)
{
    CRASH_TRACER
    auto m = markers->state.getChild (index);

    if (! m.isValid())
        return newWarpTime;

    if (index > 0)
    {
        WarpMarker* a = markers->objects.getUnchecked (index - 1);
        WarpMarker* b = markers->objects.getUnchecked (index);
        double srcLen = b->sourceTime - a->sourceTime;
        double stretchRatio = (newWarpTime - a->warpTime) / srcLen;

        if (stretchRatio < 0.10001)
            newWarpTime = 0.10001 * srcLen + a->warpTime;
        else if (stretchRatio > 19.9999)
            newWarpTime = 19.9999 * srcLen + a->warpTime;
    }

    if (index < markers->objects.size() - 1)
    {
        WarpMarker* a = markers->objects.getUnchecked (index);
        WarpMarker* b = markers->objects.getUnchecked (index + 1);
        double srcLen = b->sourceTime - a->sourceTime;
        double stretchRatio = (b->warpTime - newWarpTime) / srcLen;

        if (stretchRatio < 0.10001)
            newWarpTime = b->warpTime - 0.10001 * srcLen;
        else if (stretchRatio > 19.9999)
            newWarpTime = b->warpTime - 19.9999 * srcLen;
    }

    if (endMarkersLimited && (index == 0 || (index == markers->objects.size() - 1)))
        newWarpTime = jlimit (0.0, getSourceLength(), newWarpTime);

    m.setProperty (IDs::warpTime, newWarpTime, getUndoManager());

    return newWarpTime;
}

void WarpTimeManager::setWarpEndMarkerTime (double endTime)
{
    if (endTime > 0.0)
        state.setProperty (IDs::warpEndMarkerTime, endTime, &edit.getUndoManager());
}

Array<EditTimeRange> WarpTimeManager::getWarpTimeRegions (const EditTimeRange overallTimeRegion) const
{
    Array<EditTimeRange> visibleWarpRegions;
    auto& markersArray = getMarkers();

    if (markersArray.isEmpty())
    {
        visibleWarpRegions.add (overallTimeRegion);
        return visibleWarpRegions;
    }

    auto timeRegion = overallTimeRegion;
    double overallTime = 0.0;
    double warpedClipLength = getWarpedEnd();

    // trim this region to the end of the clip content.
    if (timeRegion.getEnd() > warpedClipLength)
        timeRegion.end = warpedClipLength;

    //set up the warp regions
    EditTimeRange warpRegion (overallTimeRegion.getStart(), warpedClipLength);

    for (int markerIndex = 0; markerIndex <= markersArray.size(); markerIndex++)
    {
        if (markerIndex == markersArray.size()) // if we're on the last region
            warpRegion.end = jmax (warpRegion.start, warpedClipLength);
        else
            warpRegion.end = jmax (warpRegion.start, markersArray.getUnchecked (markerIndex)->warpTime);

        auto warpRegionConstrained = timeRegion.getIntersectionWith (warpRegion);

        if (warpRegionConstrained.getLength() > 0) // don't add zero length regions
        {
            visibleWarpRegions.add (warpRegionConstrained);
            overallTime += warpRegionConstrained.getLength();
        }

        warpRegion.start = warpRegion.end;
    }

    timeRegion.start = overallTimeRegion.start + overallTime;
    timeRegion.end = overallTimeRegion.end;

    return visibleWarpRegions;
}

double WarpTimeManager::warpTimeToSourceTime (double warpTime) const
{
    auto& markersArray = getMarkers();

    if (markersArray.isEmpty())
        return warpTime;

    WarpMarker startMarker;
    WarpMarker endMarker;

    const WarpMarker first  = *markersArray.getFirst();
    const WarpMarker last   = *markersArray.getLast();

    if (warpTime <= first.warpTime) //below or on the 1st marker
    {
        startMarker = WarpMarker (0, 0);
        endMarker = first;
    }
    else if (warpTime > last.warpTime) // after the last marker
    {
        startMarker = last;
        const double lib = clip->getSourceLength();
        endMarker = WarpMarker (lib, lib);
    }
    else
    {
        int index = 0;
        const int numMarkers = markersArray.size();

        while (index < numMarkers && markersArray.getUnchecked (index)->warpTime < warpTime)
            index++;

        if (index > 0)
            startMarker = *markersArray.getUnchecked (index - 1);

        endMarker = *markersArray.getUnchecked (index);
    }

    const WarpMarker markerRanges (endMarker.sourceTime - startMarker.sourceTime, endMarker.warpTime - startMarker.warpTime);

    double sourcePosition = 0.0;

    if (markerRanges.warpTime == 0.0)
    {
        sourcePosition = 0.0;
    }
    else
    {
        const double warpProportion = (warpTime - startMarker.warpTime) / markerRanges.warpTime;
        sourcePosition = warpProportion * markerRanges.sourceTime + startMarker.sourceTime;
    }

    return sourcePosition;
}

double WarpTimeManager::sourceTimeToWarpTime (double sourceTime) const
{
    auto& markersArray = getMarkers();

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

    EditTimeRange source (before == nullptr ? 0.0 : before->sourceTime,
                          after == nullptr ? getSourceLength() : after->sourceTime);

    if (source.getLength() == 0.0)
        return sourceTime;

    auto prop = (sourceTime - source.getStart()) / source.getLength();

    EditTimeRange warped (before == nullptr ? 0.0 : before->warpTime,
                          after == nullptr ? getWarpedEnd() : after->warpTime);

    return warped.getStart() + (prop * warped.getLength());
}

double WarpTimeManager::getWarpedStart() const
{
    jassert (getMarkers().size() != 0);

    return getMarkers().getFirst()->warpTime;
}

double WarpTimeManager::getWarpedEnd() const
{
    jassert (getMarkers().size() != 0);

    return getMarkers().getLast()->warpTime;
}

int64 WarpTimeManager::getHash() const
{
    int64 h = 0;

    for (auto wm : getMarkers())
        h ^= wm->getHash();

    h ^= hashDouble (getWarpEndMarkerTime());

    return h;
}

double WarpTimeManager::getWarpEndMarkerTime() const
{
    if (isWarpEndMarkerEnabled())
        return state.getProperty (IDs::warpEndMarkerTime, 0.0);

    return getSourceLength();
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

UndoManager* WarpTimeManager::getUndoManager() const
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
        const ScopedLock sl (warpTimeLock);

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
    const ScopedLock sl (warpTimeLock);
    jassert (! warpTimeManagers.contains (&wtm));
    warpTimeManagers.addIfNotAlreadyThere (&wtm);
}

void WarpTimeFactory::removeWarpTimeManager (WarpTimeManager& wtm)
{
    const ScopedLock sl (warpTimeLock);
    jassert (warpTimeManagers.contains (&wtm));
    warpTimeManagers.removeAllInstancesOf (&wtm);
}
