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

/**
    Handles time/pitch stretching using various supported libraries.
    
    Currently supported libraries are SoundTouch, RubberBand and Elastique.
    These libraries may require additional module config options to enable and
    fall under their own licence terms so make sure to check those before
    distributing any code.
*/
class TimeStretcher
{
public:
    //==============================================================================
    /** Creates an TimeStretcher using the default mode. */
    TimeStretcher();
    
    /** Destructor. */
    ~TimeStretcher();

    //==============================================================================
    /** Holds the various algorithms to which can be used (if enabled). */
    enum Mode
    {
        disabled = 0,                   /**< No algorithm enabled. */
        elastiqueTransient = 1,         /**< Defunct, don't use. */
        elastiqueTonal = 2,             /**< Defunct, don't use. */
        soundtouchNormal = 3,           /**< SoundTouch normal quality, lower CPU use. */
        soundtouchBetter = 4,           /**< SoundTouch better quality, higher CPU use. */
        melodyne = 5,                   /**< Melodyne, only used for clip timestretching. */
        elastiquePro = 6,               /**< Elastique Pro good all round (@see ElastiqueProOptions). */
        elastiqueEfficient = 7,         /**< Elastique lower quality and lower CPU usage. */
        elastiqueMobile = 8,            /**< Elastique lower quality and lower CPU usage, optimised for mobile. */
        elastiqueMonophonic = 9,        /**< Elastique which can sound better for monophonic sounds. */
        rubberbandMelodic = 10,         /**< RubberBand tailored to melodic sounds prioritising pitch accuracy. */
        rubberbandPercussive = 11,      /**< RubberBand tailored to percussive sounds prioritising transient accuracy. */

       #if TRACKTION_ENABLE_TIMESTRETCH_ELASTIQUE
        defaultMode = elastiquePro      /**< Default mode. */
       #elif TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND
        defaultMode = rubberbandMelodic /**< Default mode. */
       #elif TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH
        defaultMode = soundtouchBetter  /**< Default mode. */
       #else
        defaultMode = disabled          /**< Default mode. */
       #endif
    };

    //==============================================================================
    /** A set of options that can be used in conjunction with the elastiquePro Mode
        to fine tune the algorithm. See the Elastique API for more details on these options.
    */
    struct ElastiqueProOptions
    {
        /** Create a default set of options. */
        ElastiqueProOptions() = default;
        /** Create a set of options from a saved string @see toString. */
        ElastiqueProOptions (const juce::String& string);

        /** Save the current options as a string. */
        juce::String toString() const;

        /** Compare these options with another set. */
        bool operator== (const ElastiqueProOptions&) const;
        /** Compare these options with another set. */
        bool operator!= (const ElastiqueProOptions&) const;

        bool midSideStereo = false;         /**< Optomise algorthim for mid/side channel layouts. */
        bool syncTimeStrPitchShft = false;  /**< Synchronises timestretch and pitchshifting. */
        bool preserveFormants = false;      /**< Preserve formats. */
        int envelopeOrder = 64;             /**< Sets a spectral envelope shift factor. */
    };

    //==============================================================================
    /** Checks if the given mode is available for use. */
    static Mode checkModeIsAvailable (Mode);

    /** Returns the names of the availabel Modes. */
    static juce::StringArray getPossibleModes (Engine&, bool excludeMelodyne);

    /** Returns the Mode for a given name @see getPossibleModes. */
    static Mode getModeFromName (Engine&, const juce::String& name);

    /** Returns the name of a given Mode for display purposes. */
    static juce::String getNameOfMode (Mode);

    /** Returns true if the given Mode is a Melodyne mode. */
    static bool isMelodyne (Mode);

    /** Checks that the given Mode is a valid mode and not disabled. */
    static bool canProcessFor (Mode);

    //==============================================================================
    /** Initialises the TimeStretcher ready to perform timestretching.
        This must be called at least once before calling the processData methods.
        @param sourceSampleRate     The sample rate this will be processed at
        @param samplesPerBlock      The expected number of samples per process block
        @param numChannels          The number of channels to process
        @param Mode                 The Mode to enable
        @param ElastiqueProOptions  The Elastique options to use, ignored in non-ElastiquePro modes
        @param realtime             Indicates this is for real-time or offline use
    */
    void initialise (double sourceSampleRate, int samplesPerBlock,
                     int numChannels, Mode, ElastiqueProOptions, bool realtime);

    /** Returns true if this has been fully initialised. */
    bool isInitialised() const;
    
    /** Resets the TimeStretcher ready for a new set of audio data, maintains mode, speed and pitch ratios. */
    void reset();
    
    /** Sets the timestretch speed ratio and semitones pitch shift.
        @param speedRatio   The ratio for timestretched speed. 1 = no stretching, 2 = half as fast, 0.5 = twice as fast etc.
        @param semitones    The number of semitones to adjust the pitch by 0 = not shift, 12 = up one oct, -12 = down one oct etc.
    */
    bool setSpeedAndPitch (float speedRatio, float semitones);

    /** Returns the maximum number of frames that will ever be returned by getFramesNeeded.
        This can be used to size FIFOs for real-time use accordingly.
    */
    int getMaxFramesNeeded() const;
    
    /** Returns the expected number of frames required to generate some output.
        This should be queried each block and the returned number of frames be passes to processData.
    */
    int getFramesNeeded() const;
    
    /** Processes some input frames and fills some output frames with the applied speed ratio and pitch shift.
        @param inChannels   The input sample data in non-interleaved format
        @param numSamples   The number of input frames to read from inChannels
        @param outChannels  The destination for non-interleaved output samples. This should be as big as samplesPerBlock
                            passed to the constructor but it may not fill the whole buffer. In cases where less than
                            samplesPerBlock is returned, you should query getFramesNeeded and call this method again,
                            incrementing destination buffers as required.
        @returns            The number of frames read and hence written to outChannels
    */
    int processData (const float* const* inChannels, int numSamples, float* const* outChannels);

    /** Processes some input frames and fills some output frames from a pair of AudioFifos, useful for real-time use.
        @param inFifo       The input sample data to read from
        @param numSamples   The number of input frames to read from inFifo. In inFifo must have this number of frames ready
        @param outFifo      The destination for output samples. This should have at lest samplesPerBlock free.
        @returns            The number of frames read and hence written to outFifo
    */
    int processData (AudioFifo& inFifo, int numSamples, AudioFifo& outFifo);
    
    /** Flushes the end of the stream when input data is exhausted but there is still output data available.
        Once you have called this, you can no longer call processData.
        @param outChannels  The destination for non-interleaved output samples. This should be as big as samplesPerBlock
                            passed to the constructor but it may not fill the whole buffer. In cases where less than
                            samplesPerBlock is returned, you should query getFramesNeeded and call this method again,
                            incrementing destination buffers as required.
        @returns            The number of frames read and hence written to outChannels
    */
    int flush (float* const* outChannels);

    /** @internal */
    struct Stretcher;

private:
    std::unique_ptr<Stretcher> stretcher;
    int samplesPerBlockRequested = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimeStretcher)
};

}} // namespace tracktion { inline namespace engine

namespace juce
{
    template <>
    struct VariantConverter<tracktion::engine::TimeStretcher::Mode>
    {
        static tracktion::engine::TimeStretcher::Mode fromVar (const var& v)   { return (tracktion::engine::TimeStretcher::Mode) static_cast<int> (v); }
        static var toVar (tracktion::engine::TimeStretcher::Mode v)            { return static_cast<int> (v); }
    };

    template <>
    struct VariantConverter<tracktion::engine::TimeStretcher::ElastiqueProOptions>
    {
        static tracktion::engine::TimeStretcher::ElastiqueProOptions fromVar (const var& v) { return tracktion::engine::TimeStretcher::ElastiqueProOptions (v.toString()); }
        static var toVar (const tracktion::engine::TimeStretcher::ElastiqueProOptions& v)   { return v.toString(); }
    };
}
