/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class TracktionThumbnail    : public juce::AudioThumbnailBase
{
public:
    TracktionThumbnail (int originalSamplesPerThumbnailSample,
                        juce::AudioFormatManager& formatManager,
                        juce::AudioThumbnailCache& cacheToUse);

    ~TracktionThumbnail();

    void clear() override;
    void clearChannelData();

    void reset (int newNumChannels, double newSampleRate, juce::int64 totalSamplesInSource = 0) override;
    void createChannels (int length);

    //==============================================================================
    bool loadFrom (juce::InputStream& rawInput) override;
    void saveTo (juce::OutputStream& output) const override;

    //==============================================================================
    bool setSource (juce::InputSource*) override;
    void setReader (juce::AudioFormatReader*, juce::int64 hash) override;

    juce::int64 getHashCode() const override;

    void addBlock (juce::int64 startSample, const juce::AudioBuffer<float>& incoming,
                   int startOffsetInBuffer, int numSamples) override;

    //==============================================================================
    int getNumChannels() const noexcept override;
    double getTotalLength() const noexcept override;
    bool isFullyLoaded() const noexcept override;
    double getProportionComplete() const noexcept;
    juce::int64 getNumSamplesFinished() const noexcept override;
    float getApproximatePeak() const override;
    void getApproximateMinMax (double startTime, double endTime, int channelIndex,
                               float& minValue, float& maxValue) const noexcept override;

    void drawChannel (juce::Graphics&, juce::Rectangle<int> area, bool useHighRes,
                      EditTimeRange time, int channelNum, float verticalZoomFactor);

    void drawChannels (juce::Graphics&, juce::Rectangle<int> area, bool useHighRes,
                       EditTimeRange time, float verticalZoomFactor);

private:
    //==============================================================================
    juce::AudioFormatManager& formatManagerToUse;
    juce::AudioThumbnailCache& cache;

    class LevelDataSource;
    struct MinMaxValue;
    class ThumbData;
    class CachedWindow;

    std::unique_ptr<LevelDataSource> source;
    std::unique_ptr<CachedWindow> window;
    juce::OwnedArray<ThumbData> channels;

    juce::int32 samplesPerThumbSample = 0;
    juce::int64 totalSamples = 0, numSamplesFinished = 0;
    juce::int32 numChannels = 0;
    double sampleRate = 0;
    juce::CriticalSection lock;

    bool setDataSource (LevelDataSource*);
    void setLevels (const MinMaxValue* const* values, int thumbIndex, int numChans, int numValues);

    void drawChannel (juce::Graphics&, const juce::Rectangle<int>& area, double startTime,
                      double endTime, int channelNum, float verticalZoomFactor) override;
    void drawChannels (juce::Graphics&, const juce::Rectangle<int>& area, double startTimeSeconds,
                       double endTimeSeconds, float verticalZoomFactor) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TracktionThumbnail)
};

} // namespace tracktion_engine
