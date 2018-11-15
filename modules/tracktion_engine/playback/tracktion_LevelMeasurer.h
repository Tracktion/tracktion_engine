/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

struct DbTimePair
{
    juce::uint32 time = 0;
    float dB = -100.0f;
};

//==============================================================================
/**
    Monitors the levels of buffers that are passed in, and keeps peak values,
    overloads, etc., for display in a level meter component.
*/
class LevelMeasurer
{
public:
    //==============================================================================
    LevelMeasurer();
    ~LevelMeasurer();

    //==============================================================================
    void processBuffer (juce::AudioBuffer<float>& buffer, int start, int numSamples);
    void processMidi (MidiMessageArray& midiBuffer, const float* gains);
    void processMidiLevel (float level);

    void clear();
    void clearOverload();

    //==============================================================================
    enum Mode
    {
        peakMode     = 0,
        RMSMode      = 1,
        sumDiffMode  = 2
    };

    void setMode (Mode);
    Mode getMode() const noexcept                       { return mode; }

    void setShowMidi (bool showMidi);

    int getNumActiveChannels() const noexcept           { return numActiveChannels; }

    //==============================================================================
    struct Client
    {
        Client();

        void reset() noexcept;
        bool getAndClearOverload() noexcept;
        DbTimePair getAndClearMidiLevel() noexcept;
        DbTimePair getAndClearAudioLevel (int chan) noexcept;

        static constexpr auto maxNumChannels = 8;

        DbTimePair audioLevels[maxNumChannels];
        bool overload[maxNumChannels];
        DbTimePair midiLevels;
        int numChannelsUsed = 0;
        bool clearOverload = true;
    };

    //==============================================================================
    void addClient (Client&);
    void removeClient (Client&);

    void setLevelCache (float dB) noexcept              { levelCache = dB; }
    float getLevelCache() const noexcept                { return levelCache; }

private:
    Mode mode = peakMode;
    int numActiveChannels = 1;
    bool showMidi = false;
    float levelCache = -100.0f;

    juce::Array<Client*> clients;

    JUCE_DECLARE_WEAK_REFERENCEABLE(LevelMeasurer)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeasurer)
};

//==============================================================================
/** A shared level measurer that can be used by several threads to provide a
    total output level
*/
class SharedLevelMeasurer  : public LevelMeasurer,
                             public juce::ReferenceCountedObject
{
public:
    using Ptr = juce::ReferenceCountedObjectPtr<SharedLevelMeasurer>;

    void startNextBlock (double streamTime);
    void setSize (int channels, int numSamples);
    void addBuffer (const juce::AudioBuffer<float>& inBuffer, int startSample, int numSamples);

    juce::SpinLock spinLock;
    double lastStreamTime = 0;
    juce::AudioBuffer<float> sumBuffer;
};

//==============================================================================
/**
    Applies a SharedLevelMeter to the audio passing through this node
*/
class LevelMeasuringAudioNode  : public SingleInputAudioNode
{
public:
    LevelMeasuringAudioNode (SharedLevelMeasurer::Ptr, AudioNode* input);

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo&) override;
    void prepareForNextBlock (const AudioRenderContext&) override;
    void renderOver (const AudioRenderContext&) override;
    void renderAdding (const AudioRenderContext&) override;

private:
    SharedLevelMeasurer::Ptr levelMeasurer;
};

} // namespace tracktion_engine
