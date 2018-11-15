/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct TracktionThumbnail::MinMaxValue
{
    MinMaxValue() noexcept
    {
        values[0] = 0;
        values[1] = 0;
    }

    inline void set (juce::int8 newMin, juce::int8 newMax) noexcept
    {
        values[0] = newMin;
        values[1] = newMax;
    }

    inline juce::int8 getMinValue() const noexcept        { return values[0]; }
    inline juce::int8 getMaxValue() const noexcept        { return values[1]; }

    inline void setFloat (float newMin, float newMax) noexcept
    {
        values[0] = (juce::int8) juce::jlimit (-128, 127, juce::roundToInt (newMin * 127.0f));
        values[1] = (juce::int8) juce::jlimit (-128, 127, juce::roundToInt (newMax * 127.0f));

        if (values[0] == values[1])
        {
            if (values[1] == 127)
                values[0]--;
            else
                values[1]++;
        }
    }

    inline bool isNonZero() const noexcept
    {
        return values[1] > values[0];
    }

    inline int getPeak() const noexcept
    {
        return std::max (std::abs ((int) values[0]),
                         std::abs ((int) values[1]));
    }

    inline void read (juce::InputStream& input)      { input.read (values, 2); }
    inline void write (juce::OutputStream& output)   { output.write (values, 2); }

private:
    juce::int8 values[2];
};

//==============================================================================
class TracktionThumbnail::LevelDataSource   : public juce::TimeSliceClient
{
public:
    LevelDataSource (TracktionThumbnail& thumb, juce::AudioFormatReader* newReader, juce::int64 hash)
        : hashCode (hash), owner (thumb), reader (newReader)
    {
    }

    LevelDataSource (TracktionThumbnail& thumb, juce::InputSource* src)
        : hashCode (src->hashCode()), owner (thumb), source (src)
    {
    }

    ~LevelDataSource()
    {
        owner.cache.getTimeSliceThread().removeTimeSliceClient (this);
    }

    enum { timeBeforeDeletingReader = 3000 };

    void initialise (juce::int64 samplesFinished)
    {
        const juce::ScopedLock sl (readerLock);

        numSamplesFinished = samplesFinished;

        createReader();

        if (reader != nullptr)
        {
            lengthInSamples = reader->lengthInSamples;
            numChannels = reader->numChannels;
            sampleRate = reader->sampleRate;

            if (lengthInSamples <= 0 || isFullyLoaded())
                reader = nullptr;
            else
                owner.cache.getTimeSliceThread().addTimeSliceClient (this);
        }
    }

    void getLevels (juce::int64 startSample, int numSamples, juce::Array<float>& levels)
    {
        const juce::ScopedLock sl (readerLock);

        if (reader == nullptr)
        {
            createReader();

            if (reader != nullptr)
            {
                lastReaderUseTime = juce::Time::getMillisecondCounter();
                owner.cache.getTimeSliceThread().addTimeSliceClient (this);
            }
        }

        if (reader != nullptr)
        {
            float l[4] = { 0 };
            reader->readMaxLevels (startSample, numSamples, l[0], l[1], l[2], l[3]);

            levels.clearQuick();
            levels.addArray ((const float*) l, 4);

            lastReaderUseTime = juce::Time::getMillisecondCounter();
        }
    }

    void releaseResources()
    {
        const juce::ScopedLock sl (readerLock);
        reader = nullptr;
    }

    int useTimeSlice() override
    {
        if (isFullyLoaded())
        {
            if (reader != nullptr && source != nullptr)
            {
                if (juce::Time::getMillisecondCounter() > lastReaderUseTime + timeBeforeDeletingReader)
                    releaseResources();
                else
                    return 200;
            }

            return -1;
        }

        bool justFinished = false;

        {
            const juce::ScopedLock sl (readerLock);

            createReader();

            if (reader != nullptr)
            {
                if (! readNextBlock())
                    return 0;

                justFinished = true;
            }
        }

        if (justFinished)
            owner.cache.storeThumb (owner, hashCode);

        return 200;
    }

    bool isFullyLoaded() const noexcept
    {
        return numSamplesFinished >= lengthInSamples;
    }

    inline int sampleToThumbSample (juce::int64 originalSample) const noexcept
    {
        return (int) (originalSample / owner.samplesPerThumbSample);
    }

    juce::int64 lengthInSamples = 0, numSamplesFinished = 0;
    double sampleRate = 0;
    unsigned int numChannels = 0;
    juce::int64 hashCode = 0;

private:
    TracktionThumbnail& owner;
    std::unique_ptr<juce::InputSource> source;
    std::unique_ptr<juce::AudioFormatReader> reader;
    juce::CriticalSection readerLock;
    juce::uint32 lastReaderUseTime = 0;

    void createReader()
    {
        if (reader == nullptr && source != nullptr)
            if (auto* audioFileStream = source->createInputStream())
                reader.reset (owner.formatManagerToUse.createReaderFor (audioFileStream));
    }

    bool readNextBlock()
    {
        jassert (reader != nullptr);

        if (! isFullyLoaded())
        {
            auto numToDo = (int) std::min (256 * (juce::int64) owner.samplesPerThumbSample,
                                           lengthInSamples - numSamplesFinished);

            if (numToDo > 0)
            {
                auto startSample = numSamplesFinished;

                auto firstThumbIndex = sampleToThumbSample (startSample);
                auto lastThumbIndex  = sampleToThumbSample (startSample + numToDo);
                auto numThumbSamps = lastThumbIndex - firstThumbIndex;

                juce::HeapBlock<MinMaxValue> levelData ((size_t) numThumbSamps * 2);
                MinMaxValue* levels[2] = { levelData, levelData + numThumbSamps };

                for (int i = 0; i < numThumbSamps; ++i)
                {
                    float lowestLeft, highestLeft, lowestRight, highestRight;

                    reader->readMaxLevels ((firstThumbIndex + i) * owner.samplesPerThumbSample, owner.samplesPerThumbSample,
                                           lowestLeft, highestLeft, lowestRight, highestRight);

                    levels[0][i].setFloat (lowestLeft, highestLeft);
                    levels[1][i].setFloat (lowestRight, highestRight);
                }

                {
                    const juce::ScopedUnlock su (readerLock);
                    owner.setLevels (levels, firstThumbIndex, 2, numThumbSamps);
                }

                numSamplesFinished += numToDo;
                lastReaderUseTime = juce::Time::getMillisecondCounter();
            }
        }

        return isFullyLoaded();
    }
};

//==============================================================================
class TracktionThumbnail::ThumbData
{
public:
    ThumbData (int numThumbSamples)
    {
        ensureSize (numThumbSamples);
    }

    inline MinMaxValue* getData (int thumbSampleIndex) noexcept
    {
        jassert (thumbSampleIndex < data.size());
        return data.getRawDataPointer() + thumbSampleIndex;
    }

    int getSize() const noexcept
    {
        return data.size();
    }

    void getMinMax (int startSample, int endSample, MinMaxValue& result) const noexcept
    {
        if (startSample >= 0)
        {
            endSample = std::min (endSample, data.size() - 1);

            juce::int8 mx = -128, mn = 127;

            while (startSample <= endSample)
            {
                const MinMaxValue& v = data.getReference (startSample);

                if (v.getMinValue() < mn)  mn = v.getMinValue();
                    if (v.getMaxValue() > mx)  mx = v.getMaxValue();

                        ++startSample;
            }

            if (mn <= mx)
            {
                result.set (mn, mx);
                return;
            }
        }

        result.set (1, 0);
    }

    void write (const MinMaxValue* values, int startIndex, int numValues)
    {
        resetPeak();

        if (startIndex + numValues > data.size())
            ensureSize (startIndex + numValues);

        MinMaxValue* const dest = getData (startIndex);

        for (int i = 0; i < numValues; ++i)
            dest[i] = values[i];
    }

    void resetPeak() noexcept
    {
        peakLevel = -1;
    }

    int getPeak() noexcept
    {
        if (peakLevel < 0)
        {
            for (int i = 0; i < data.size(); ++i)
            {
                const int peak = data[i].getPeak();

                if (peak > peakLevel)
                    peakLevel = peak;
            }
        }

        return peakLevel;
    }

private:
    juce::Array<MinMaxValue> data;
    int peakLevel = -1;

    void ensureSize (int thumbSamples)
    {
        const int extraNeeded = thumbSamples - data.size();

        if (extraNeeded > 0)
            data.insertMultiple (-1, MinMaxValue(), extraNeeded);
    }
};

//==============================================================================
class TracktionThumbnail::CachedWindow
{
public:
    CachedWindow() {}

    void invalidate()
    {
        cacheNeedsRefilling = true;
    }

    void drawChannel (juce::Graphics& g, juce::Rectangle<int> area, bool useHighRes,
                      EditTimeRange time, int channelNum, float verticalZoomFactor,
                      double rate, int numChans, int sampsPerThumbSample,
                      LevelDataSource* levelData, const juce::OwnedArray<ThumbData>& chans)
    {
        if (refillCache (area.getWidth(), time, rate,
                         numChans, sampsPerThumbSample, levelData, chans)
            && juce::isPositiveAndBelow (channelNum, numChannelsCached))
        {
            auto clip = g.getClipBounds().withTrimmedRight (useHighRes ? -1 : 0)
                                         .getIntersection (area.withWidth (std::min (numSamplesCached, area.getWidth())));

            if (! clip.isEmpty())
            {
                auto topY = (float) area.getY();
                auto bottomY = (float) area.getBottom();
                auto midY = (topY + bottomY) * 0.5f;
                auto vscale = verticalZoomFactor * (bottomY - topY) / 256.0f;

                auto* cacheData = getData (channelNum, clip.getX() - area.getX());

                auto x = (float) clip.getX();

                if (useHighRes)
                {
                    float lastY = std::max (midY - cacheData->getMaxValue() * vscale - 0.3f, topY);
                    bool drewLast = true;

                    juce::Path p;
                    p.preallocateSpace (clip.getWidth() * 2 * 2 + 3);
                    p.startNewSubPath (x, lastY);
                    x += 1.0f;
                    ++cacheData;

                    for (int w = clip.getWidth() - 1; --w >= 0;)
                    {
                        if (cacheData->isNonZero())
                        {
                            auto top = std::max (midY - cacheData->getMaxValue() * vscale - 0.3f, topY);

                            if (top != lastY || w == 0)
                            {
                                if (! drewLast)
                                    p.lineTo (x - 1.0f, lastY);

                                p.lineTo (x, top);
                                drewLast = true;
                            }
                            else
                            {
                                drewLast = false;
                            }

                            lastY = top;
                        }

                        x += 1.0f;
                        ++cacheData;
                    }

                    for (int w = 0; ++w <= clip.getWidth();)
                    {
                        x -= 1.0f;
                        --cacheData;

                        if (cacheData->isNonZero())
                        {
                            auto bottom = std::min (midY - cacheData->getMinValue() * vscale + 0.3f, bottomY);

                            if (bottom != lastY || w == clip.getWidth())
                            {
                                if (! drewLast)
                                    p.lineTo (x + 1, lastY);

                                p.lineTo (x, bottom);
                                drewLast = true;
                            }
                            else
                            {
                                drewLast = false;
                            }

                            lastY = bottom;
                        }
                    }

                    p.closeSubPath();
                    g.fillPath (p);
                }
                else
                {
                    juce::RectangleList<float> waveform;
                    waveform.ensureStorageAllocated (clip.getWidth());

                    for (int w = clip.getWidth(); --w >= 0;)
                    {
                        if (cacheData->isNonZero())
                        {
                            auto top    = std::max (midY - cacheData->getMaxValue() * vscale - 0.3f, topY);
                            auto bottom = std::min (midY - cacheData->getMinValue() * vscale + 0.3f, bottomY);

                            waveform.addWithoutMerging ({ x, top, 1.0f, bottom - top });
                        }

                        x += 1.0f;
                        ++cacheData;
                    }

                    g.fillRectList (waveform);
                }
            }
        }
    }

private:
    juce::Array<MinMaxValue> data;
    double cachedStart = 0, cachedTimePerPixel = 0;
    int numChannelsCached = 0, numSamplesCached = 0;
    bool cacheNeedsRefilling = true;

    bool refillCache (int numSamples, EditTimeRange time,
                      double rate, int numChans, int sampsPerThumbSample,
                      LevelDataSource* levelData, const juce::OwnedArray<ThumbData>& chans)
    {
        auto timePerPixel = time.getLength() / numSamples;

        if (numSamples <= 0 || timePerPixel <= 0.0 || rate <= 0)
        {
            invalidate();
            return false;
        }

        if (numSamples == numSamplesCached
             && numChannelsCached == numChans
             && time.getStart() == cachedStart
             && timePerPixel == cachedTimePerPixel
             && ! cacheNeedsRefilling)
        {
            return ! cacheNeedsRefilling;
        }

        auto startTime = time.getStart();

        numSamplesCached = numSamples;
        numChannelsCached = numChans;
        cachedStart = startTime;
        cachedTimePerPixel = timePerPixel;
        cacheNeedsRefilling = false;

        ensureSize (numSamples);

        if (timePerPixel * rate <= sampsPerThumbSample && levelData != nullptr)
        {
            auto sample = juce::roundToInt (startTime * rate);
            juce::Array<float> levels, lastLevels;
            lastLevels.insertMultiple (0, 0.0f, numChannelsCached * 2);
            int i;

            for (i = 0; i < numSamples; ++i)
            {
                auto nextSample = juce::roundToInt ((startTime + timePerPixel) * rate);

                if (sample >= 0)
                {
                    if (sample >= levelData->lengthInSamples)
                    {
                        for (int chan = 0; chan < numChannelsCached; ++chan)
                            *getData (chan, i) = MinMaxValue();
                    }
                    else
                    {
                        levelData->getLevels (sample, std::max (1, nextSample - sample), levels);

                        auto totalChans = std::min (levels.size() / 2, numChannelsCached);

                        for (int chan = 0; chan < totalChans; ++chan)
                        {
                            int c1 = chan * 2;
                            int c2 = chan * 2 + 1;
                            float chan1 = levels.getUnchecked (c1);
                            float chan2 = levels.getUnchecked (c2);

                            getData (chan, i)->setFloat (std::min (chan1, lastLevels[c2]),
                                                         std::max (chan2, lastLevels[c1]));

                            lastLevels.getReference (c1) = chan1;
                            lastLevels.getReference (c2) = chan2;
                        }
                    }
                }

                startTime += timePerPixel;
                sample = nextSample;
            }

            numSamplesCached = i;
        }
        else
        {
            jassert (chans.size() == numChannelsCached);

            for (int channelNum = 0; channelNum < numChannelsCached; ++channelNum)
            {
                ThumbData* channelData = chans.getUnchecked (channelNum);
                MinMaxValue* cacheData = getData (channelNum, 0);

                auto timeToThumbSampleFactor = rate / (double) sampsPerThumbSample;

                startTime = cachedStart;
                auto sample = juce::roundToInt (startTime * timeToThumbSampleFactor);

                for (int i = numSamples; --i >= 0;)
                {
                    auto nextSample = juce::roundToInt ((startTime + timePerPixel) * timeToThumbSampleFactor);

                    channelData->getMinMax (sample, nextSample, *cacheData);

                    ++cacheData;
                    startTime += timePerPixel;
                    sample = nextSample;
                }
            }
        }

        return true;
    }

    MinMaxValue* getData (int channelNum, int cacheIndex) noexcept
    {
        jassert (juce::isPositiveAndBelow (channelNum, numChannelsCached)
                  && juce::isPositiveAndBelow (cacheIndex, data.size()));

        return data.getRawDataPointer() + channelNum * numSamplesCached + cacheIndex;
    }

    void ensureSize (int numSamples)
    {
        auto itemsRequired = numSamples * numChannelsCached;

        if (data.size() < itemsRequired)
            data.insertMultiple (-1, MinMaxValue(), itemsRequired - data.size());
    }
};

//==============================================================================
TracktionThumbnail::TracktionThumbnail (int originalSamplesPerThumbnailSample,
                                        juce::AudioFormatManager& formatManager,
                                        juce::AudioThumbnailCache& cacheToUse)
    : formatManagerToUse (formatManager),
      cache (cacheToUse),
      window (new CachedWindow()),
      samplesPerThumbSample (originalSamplesPerThumbnailSample)
{
}

TracktionThumbnail::~TracktionThumbnail()
{
    clear();
}

void TracktionThumbnail::clear()
{
    source = nullptr;
    const juce::ScopedLock sl (lock);
    clearChannelData();
}

void TracktionThumbnail::clearChannelData()
{
    window->invalidate();
    channels.clear();
    totalSamples = numSamplesFinished = 0;
    numChannels = 0;
    sampleRate = 0;

    sendChangeMessage();
}

void TracktionThumbnail::reset (int newNumChannels, double newSampleRate, juce::int64 totalSamplesInSource)
{
    clear();

    const juce::ScopedLock sl (lock);
    numChannels = newNumChannels;
    sampleRate = newSampleRate;
    totalSamples = totalSamplesInSource;

    createChannels (1 + (int) (totalSamplesInSource / samplesPerThumbSample));
}

void TracktionThumbnail::createChannels (int length)
{
    while (channels.size() < numChannels)
        channels.add (new ThumbData (length));
}

//==============================================================================
bool TracktionThumbnail::loadFrom (juce::InputStream& rawInput)
{
    juce::BufferedInputStream input (rawInput, 4096);

    if (input.readByte() != 'j' || input.readByte() != 'a' || input.readByte() != 't' || input.readByte() != 'm')
        return false;

    const juce::ScopedLock sl (lock);
    clearChannelData();

    samplesPerThumbSample = input.readInt();
    totalSamples = input.readInt64();             // Total number of source samples.
    numSamplesFinished = input.readInt64();       // Number of valid source samples that have been read into the thumbnail.
    auto numThumbnailSamples = input.readInt();   // Number of samples in the thumbnail data.
    numChannels = input.readInt();                // Number of audio channels.
    sampleRate = input.readInt();                 // Source sample rate.
    input.skipNextBytes (16);                     // (reserved)

    createChannels (numThumbnailSamples);

    for (int i = 0; i < numThumbnailSamples; ++i)
        for (int chan = 0; chan < numChannels; ++chan)
            channels.getUnchecked(chan)->getData(i)->read (input);

    return true;
}

void TracktionThumbnail::saveTo (juce::OutputStream& output) const
{
    const juce::ScopedLock sl (lock);

    const int numThumbnailSamples = channels.isEmpty() ? 0 : channels.getUnchecked(0)->getSize();

    output.write ("jatm", 4);
    output.writeInt (samplesPerThumbSample);
    output.writeInt64 (totalSamples);
    output.writeInt64 (numSamplesFinished);
    output.writeInt (numThumbnailSamples);
    output.writeInt (numChannels);
    output.writeInt ((int) sampleRate);
    output.writeInt64 (0);
    output.writeInt64 (0);

    for (int i = 0; i < numThumbnailSamples; ++i)
        for (int chan = 0; chan < numChannels; ++chan)
            channels.getUnchecked(chan)->getData(i)->write (output);
}

//==============================================================================
bool TracktionThumbnail::setDataSource (LevelDataSource* newSource)
{
    jassert (juce::MessageManager::getInstance()->currentThreadHasLockedMessageManager());

    numSamplesFinished = 0;

    if (cache.loadThumb (*this, newSource->hashCode) && isFullyLoaded())
    {
        source.reset (newSource); // (make sure this isn't done before loadThumb is called)

        source->lengthInSamples = totalSamples;
        source->sampleRate = sampleRate;
        source->numChannels = (unsigned int) numChannels;
        source->numSamplesFinished = numSamplesFinished;
    }
    else
    {
        source.reset (newSource); // (make sure this isn't done before loadThumb is called)

        const juce::ScopedLock sl (lock);
        source->initialise (numSamplesFinished);

        totalSamples = source->lengthInSamples;
        sampleRate = source->sampleRate;
        numChannels = (juce::int32) source->numChannels;

        createChannels (1 + (int) (totalSamples / samplesPerThumbSample));
    }

    return sampleRate > 0 && totalSamples > 0;
}

bool TracktionThumbnail::setSource (juce::InputSource* newSource)
{
    clear();

    return newSource != nullptr && setDataSource (new LevelDataSource (*this, newSource));
}

void TracktionThumbnail::setReader (juce::AudioFormatReader* newReader, juce::int64 hash)
{
    clear();

    if (newReader != nullptr)
        setDataSource (new LevelDataSource (*this, newReader, hash));
}

juce::int64 TracktionThumbnail::getHashCode() const
{
    return source == nullptr ? 0 : source->hashCode;
}

void TracktionThumbnail::addBlock (juce::int64 startSample, const juce::AudioBuffer<float>& incoming,
                                   int startOffsetInBuffer, int numSamples)
{
    jassert (startSample >= 0);

    auto firstThumbIndex = (int) (startSample / samplesPerThumbSample);
    auto lastThumbIndex  = (int) ((startSample + numSamples + (samplesPerThumbSample - 1)) / samplesPerThumbSample);
    auto numToDo = lastThumbIndex - firstThumbIndex;

    if (numToDo > 0)
    {
        auto numChans = std::min (channels.size(), incoming.getNumChannels());

        const juce::HeapBlock<MinMaxValue> thumbData ((size_t) (numToDo * numChans));
        const juce::HeapBlock<MinMaxValue*> thumbChannels ((size_t) numChans);

        for (int chan = 0; chan < numChans; ++chan)
        {
            const float* const sourceData = incoming.getReadPointer (chan, startOffsetInBuffer);
            auto* dest = thumbData + numToDo * chan;
            thumbChannels[chan] = dest;

            for (int i = 0; i < numToDo; ++i)
            {
                const int start = i * samplesPerThumbSample;
                auto range = juce::FloatVectorOperations::findMinAndMax (sourceData + start, std::min (samplesPerThumbSample, numSamples - start));
                dest[i].setFloat (range.getStart(), range.getEnd());
            }
        }

        setLevels (thumbChannels, firstThumbIndex, numChans, numToDo);
    }
}

void TracktionThumbnail::setLevels (const MinMaxValue* const* values, int thumbIndex, int numChans, int numValues)
{
    const juce::ScopedLock sl (lock);

    for (int i = std::min (numChans, channels.size()); --i >= 0;)
        channels.getUnchecked(i)->write (values[i], thumbIndex, numValues);

    auto start = thumbIndex * (juce::int64) samplesPerThumbSample;
    auto end = (thumbIndex + numValues) * (juce::int64) samplesPerThumbSample;

    if (numSamplesFinished >= start && end > numSamplesFinished)
        numSamplesFinished = end;

    totalSamples = std::max (numSamplesFinished, totalSamples);
    window->invalidate();
    sendChangeMessage();
}

//==============================================================================
int TracktionThumbnail::getNumChannels() const noexcept
{
    return numChannels;
}

double TracktionThumbnail::getTotalLength() const noexcept
{
    return sampleRate > 0 ? (totalSamples / sampleRate) : 0;
}

bool TracktionThumbnail::isFullyLoaded() const noexcept
{
    return numSamplesFinished >= totalSamples - samplesPerThumbSample;
}

double TracktionThumbnail::getProportionComplete() const noexcept
{
    return juce::jlimit (0.0, 1.0, numSamplesFinished / (double) std::max ((juce::int64) 1, totalSamples));
}

juce::int64 TracktionThumbnail::getNumSamplesFinished() const noexcept
{
    return numSamplesFinished;
}

float TracktionThumbnail::getApproximatePeak() const
{
    const juce::ScopedLock sl (lock);
    int peak = 0;

    for (int i = channels.size(); --i >= 0;)
        peak = std::max (peak, channels.getUnchecked(i)->getPeak());

    return juce::jlimit (0, 127, peak) / 127.0f;
}

void TracktionThumbnail::getApproximateMinMax (double startTime, double endTime, int channelIndex,
                                               float& minValue, float& maxValue) const noexcept
{
    const juce::ScopedLock sl (lock);
    MinMaxValue result;
    auto* data = channels[channelIndex];

    if (data != nullptr && sampleRate > 0)
    {
        auto firstThumbIndex = (int) ((startTime * sampleRate) / samplesPerThumbSample);
        auto lastThumbIndex  = (int) (((endTime * sampleRate) + samplesPerThumbSample - 1) / samplesPerThumbSample);

        data->getMinMax (std::max (0, firstThumbIndex), lastThumbIndex, result);
    }

    minValue = result.getMinValue() / 128.0f;
    maxValue = result.getMaxValue() / 128.0f;
}

void TracktionThumbnail::drawChannel (juce::Graphics& g, const juce::Rectangle<int>& area, double start, double end, int channel, float zoom)
{
    drawChannel (g, area, true, { start, end }, channel, zoom);
}

void TracktionThumbnail::drawChannels (juce::Graphics& g, const juce::Rectangle<int>& area, double start, double end, float zoom)
{
    drawChannels (g, area, true, { start, end }, zoom);
}

void TracktionThumbnail::drawChannel (juce::Graphics& g, juce::Rectangle<int> area, bool useHighRes,
                                      EditTimeRange time, int channelNum, float verticalZoomFactor)
{
    const juce::ScopedLock sl (lock);

    window->drawChannel (g, area, useHighRes, time, channelNum, verticalZoomFactor,
                         sampleRate, numChannels, samplesPerThumbSample, source.get(), channels);
}

void TracktionThumbnail::drawChannels (juce::Graphics& g, juce::Rectangle<int> area, bool useHighRes,
                                       EditTimeRange time, float verticalZoomFactor)
{
    for (int i = 0; i < numChannels; ++i)
    {
        auto y1 = juce::roundToInt ((i * area.getHeight()) / numChannels);
        auto y2 = juce::roundToInt (((i + 1) * area.getHeight()) / numChannels);

        drawChannel (g, { area.getX(), area.getY() + y1, area.getWidth(), y2 - y1 }, useHighRes,
                     time, i, verticalZoomFactor);
    }
}
