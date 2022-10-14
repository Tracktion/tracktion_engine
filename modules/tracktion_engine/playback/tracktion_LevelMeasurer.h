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

struct DbTimePair
{
    uint32_t time = 0;
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
    void clearPeak();

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
        Client() = default;

        void reset() noexcept;
        bool getAndClearOverload() noexcept;
        bool getAndClearPeak() noexcept;
        DbTimePair getAndClearMidiLevel() noexcept;
        DbTimePair getAndClearAudioLevel (int chan) noexcept;

        static constexpr auto maxNumChannels = 8;

        /** @internal */
        void setNumChannelsUsed (int) noexcept;
        void setOverload (int channel, bool hasOverloaded) noexcept;
        void setClearOverload (bool) noexcept;
        void setClearPeak (bool) noexcept;

        void updateAudioLevel (int channel, DbTimePair) noexcept;
        void updateMidiLevel (DbTimePair) noexcept;

    private:
        DbTimePair audioLevels[maxNumChannels];
        bool overload[maxNumChannels] = {};
        DbTimePair midiLevels;
        int numChannelsUsed = 0;
        bool clearOverload = true;
        bool clearPeak = true;

        juce::SpinLock mutex;
    };

    //==============================================================================
    void addClient (Client&);
    void removeClient (Client&);

    void setLevelCache (float dBL, float dBR) noexcept      { levelCacheL = dBL; levelCacheR = dBR; }
    std::pair<float, float> getLevelCache() const noexcept  { return { levelCacheL, levelCacheR }; }

private:
    Mode mode = peakMode;
    int numActiveChannels = 1;
    bool showMidi = false;
    float levelCacheL = -100.0f;
    float levelCacheR = -100.0f;

    juce::Array<Client*> clients;
    juce::CriticalSection clientsMutex;

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

}} // namespace tracktion { inline namespace engine
