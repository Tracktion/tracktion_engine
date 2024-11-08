/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine
{

//==============================================================================
//==============================================================================
/**
    Wraps a TimeStretcher but keeps a larger internal input and output buffer
    and uses a background thread to try and process frames, reducing CPU cost on
    real-time threads.
 */
class ReadAheadTimeStretcher
{
public:
    /** Creates a ReadAheadTimeStretcher that will attempt to process the desired number
        of blocks on a background thread.
       The number of blocks is a multiple of that passed to the initalise call.
    */
    ReadAheadTimeStretcher (int numBlocksToReadAhead);

    /** Destructor. */
    ~ReadAheadTimeStretcher();

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
                     int numChannels, TimeStretcher::Mode, TimeStretcher::ElastiqueProOptions,
                     bool realtime);

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
        This should be queried each block and the returned number of frames be passed to pushData.
    */
    int getFramesNeeded() const;

    /** Returns the number of frames that should be pushed to ensure the background thread has ample to work with.
        This also prioritises a fast startup where possible after a reset()
    */
    int getFramesRecomended() const;

    /** Returns true if more frames are required to be pushed in order for a pop operation to succeed. */
    bool requiresMoreFrames() const;

    /** Returns the number of samples that can be pushed to the input, ready to be processed. */
    int getFreeSpace() const;

    /** Pushes a number of samples to the instance ready to be time-stretched.
        N.B. This doesn't actually do any processing so should be quick.
        @returns The number of samples read
     */
    int pushData (const float* const* inChannels, int numSamples);

    /** Returns the number of samples ready in the output buffer which can be
        retrieved without processing the time-stretch.
    */
    int getNumReady() const;

    /** Retrieves some samples from the output buffer.
        N.B. If there aren't enough samples already processed this will do the processing so its cost can vary.
        @returns The number of samples read
    */
    int popData (float* const* outChannels, int numSamples);

    /** Flushes the end of the stream when input data is exhausted but there is still output data available.
        Once you have called this, you can no longer call processData.
        @param outChannels  The destination for non-interleaved output samples. This should be as big as samplesPerBlock
                            passed to the constructor but it may not fill the whole buffer. In cases where less than
                            samplesPerBlock is returned, you should query getFramesNeeded and call this method again,
                            incrementing destination buffers as required.
        @returns            The number of frames read and hence written to outChannels
    */
    int flush (float* const* outChannels);

private:
    //==============================================================================
    class ProcessThread
    {
    public:
        ProcessThread();
        ~ProcessThread();

        void addInstance (ReadAheadTimeStretcher*);
        void removeInstance (ReadAheadTimeStretcher*);

        void flagForProcessing (std::atomic<std::uint64_t>&);

    private:
        //==============================================================================
        std::vector<ReadAheadTimeStretcher*> instances;
        std::mutex instancesMutex;

        std::thread thread;
        juce::WaitableEvent event;
        std::atomic_flag waitingToExitFlag = ATOMIC_FLAG_INIT;
        std::atomic_flag breakForAddRemoveInstance = ATOMIC_FLAG_INIT;
        std::atomic<std::uint64_t> processEpoch { 0 };

        //==============================================================================
        void process();
    };

    AudioFifo inputFifo { 1, 32 }, outputFifo { 1, 32 };
    mutable TimeStretcher stretcher;
    const int numBlocksToReadAhead;
    int numChannels = 0, numSamplesPerOutputBlock = 0;
    mutable std::mutex processMutex;
    std::atomic<std::uint64_t> epoch { std::numeric_limits<std::uint64_t>::max() };

    mutable std::atomic<float> pendingSpeedRatio { 1.0f }, pendingSemitonesUp { 0.0f };
    mutable std::atomic<bool> newSpeedAndPitchPending { false }, hasBeenReset { true };

    juce::SharedResourcePointer<ProcessThread> processThread;

    void tryToSetNewSpeedAndPitch() const;
    int processNextBlock (bool shouldBlock);
    bool canProcessNextBlock();
    std::uint64_t getEpoch() const { return epoch.load (std::memory_order_acquire); }
};

}
