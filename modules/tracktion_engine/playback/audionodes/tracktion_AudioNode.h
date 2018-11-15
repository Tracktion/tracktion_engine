/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** Holds some really basic properties of a node */
struct AudioNodeProperties
{
    bool hasAudio;
    bool hasMidi;
    int numberOfChannels;
};

//==============================================================================
/** Passed into AudioNodes when they are being initialised, to give them useful
    contextual information that they may need
*/
struct PlaybackInitialisationInfo
{
    double startTime;
    double sampleRate;
    int blockSizeSamples;
    const juce::Array<AudioNode*>* rootNodes;
    PlayHead& playhead;
};

//==============================================================================
/** A collection of settings that are generally needed when asking edit model
    objects to create AudioNodes to render them.
*/
struct CreateAudioNodeParams
{
    const juce::Array<Clip*>* allowedClips = nullptr;
    const juce::BigInteger* allowedTracks = nullptr;
    AudioNode* audioNodeToBeReplaced = nullptr;
    bool forRendering = false;
    bool includePlugins = true;
    bool addAntiDenormalisationNoise = false;
};

//==============================================================================
/** Rendering target info.

    If addValuesToExistingBufferContents is true, the method must add its data to the
    buffer's contents, otherwise it must overwrite it, as the buffers' contents will
    be undefined.

    Any midi messages added to the midi buffer must have timestamps relative to
    the start of this block + the midiBufferOffset value.

    Either the audio or midi buffers passed in may be null if that kind of data isn't
    required by the caller.
*/
struct AudioRenderContext
{
    //==============================================================================
    inline AudioRenderContext (PlayHead& ph, EditTimeRange stream,
                               juce::AudioBuffer<float>* buffer,
                               const juce::AudioChannelSet& bufferChannels,
                               int bufferStart, int bufferSize,
                               MidiMessageArray* midiBuffer, double midiOffset,
                               int continuityFlags, bool rendering) noexcept
        : playhead (ph), streamTime (stream),
          destBuffer (buffer), destBufferChannels (bufferChannels),
          bufferStartSample (bufferStart), bufferNumSamples (bufferSize),
          bufferForMidiMessages (midiBuffer), midiBufferOffset (midiOffset),
          continuity (continuityFlags), isRendering (rendering)
    {}

    AudioRenderContext (const AudioRenderContext&) = default;
    AudioRenderContext (AudioRenderContext&&) = default;
    AudioRenderContext& operator= (const AudioRenderContext&) = default;
    AudioRenderContext& operator= (AudioRenderContext&&) = default;

    //==============================================================================
    /** The playhead provides information about current time, tempo etc at the block
        being rendered.
    */
    PlayHead& playhead;

    /** The time window which needs to be rendered into the current block.
        This is a monotonically increasing window, even if playback is paused. To find
        out what section of the edit needs to be rendered, Playhead provides conversion
        methods such as Playhead::streamTimeToEditWindow() or getEditTime()
    */
    EditTimeRange streamTime;

    /** The target audio buffer which needs to be filled.
        This may be nullptr if no audio is being processed.
    */
    juce::AudioBuffer<float>* destBuffer;

    /** A description of the type of channels in each of the channels in destBuffer. */
    juce::AudioChannelSet destBufferChannels;

    /** The index of the start point in the audio buffer from which data must be written. */
    int bufferStartSample;

    /** The number of samples to write into the audio buffer. */
    int bufferNumSamples;

    /** A buffer of MIDI events to process.
        This may be nullptr if no MIDI is being sent
    */
    MidiMessageArray* bufferForMidiMessages;

    /** A time offset to add to the timestamp of any events in the MIDI buffer. */
    double midiBufferOffset;

    /** Values used in the AudioRenderContext::continuity variable */
    enum ContinuityFlags
    {
        contiguous           = 1,
        playheadJumped       = 2,
        lastBlockBeforeLoop  = 4,
        firstBlockOfLoop     = 8
    };

    /** A set of flags to indicate what the relationship is between this block and the previous one.
        @see ContinuityFlags
    */
    int continuity;

    bool isContiguousWithPreviousBlock() const noexcept     { return (continuity & contiguous) != 0; }
    bool isFirstBlockOfLoop() const noexcept                { return (continuity & firstBlockOfLoop) != 0; }
    bool isLastBlockOfLoop() const noexcept                 { return (continuity & lastBlockBeforeLoop) != 0; }
    bool didPlayheadJump() const noexcept                   { return (continuity & playheadJumped) != 0; }

    /** True if the rendering is happening as part of an offline render rather than live playback. */
    bool isRendering;

    //==============================================================================
    /** Returns the section of the edit that needs to be rendered by this block. */
    PlayHead::EditTimeWindow getEditTime() const;

    /** Clears the active section of all channels in the audio buffer. */
    void clearAudioBuffer() const noexcept;
    /** Clears the active section of the MIDI buffer. */
    void clearMidiBuffer() const noexcept;
    /** Clears the active section of all channels in the audio and MIDI buffers. */
    void clearAll() const noexcept;
    /** Applies low-level noise to the audio buffer. */
    void addAntiDenormalisationNoise() const noexcept;

    /** Does a quick check on the bounds of various values in the structure */
    void sanityCheck() const
    {
        jassert (destBuffer == nullptr || bufferStartSample + bufferNumSamples <= destBuffer->getNumSamples());
    }

    /** Ensures that the buffer has at least the requested number of channels */
    void setMaxNumChannels (int num) const
    {
        jassert (destBuffer != nullptr);
        destBuffer->setSize (juce::jmin (destBuffer->getNumChannels(), num), destBuffer->getNumSamples(), true);
    }
};

//==============================================================================
/**
    Base class for nodes in an audio playback graph.
*/
class AudioNode
{
public:
    //==============================================================================
    AudioNode();
    virtual ~AudioNode();

    //==============================================================================
    virtual void getAudioNodeProperties (AudioNodeProperties&) = 0;

    /** tells the node to initialise itself ready for playing from the given time.
        This call may be made more than once before releaseAudioNodeResources() is called
    */
    virtual void prepareAudioNodeToPlay (const PlaybackInitialisationInfo&) = 0;

    /** Tells the node to delete any sub-nodes that don't produce the required type of output.
        This optimises out any unplayable bits of the audio graph.
        Returns true if this node should be kept, false if this node can itself be deleted.
    */
    virtual bool purgeSubNodes (bool keepAudio, bool keepMidi) = 0;

    /** tells the node that play has stopped, and it can free up anything it no longer needs.
    */
    virtual void releaseAudioNodeResources() = 0;

    //==============================================================================
    using VisitorFn = std::function<void(AudioNode&)>;
    virtual void visitNodes (const VisitorFn&) = 0;

    virtual juce::ReferenceCountedObjectPtr<Plugin> getPlugin() const      { return {}; }

    // called before renderOver/Adding, to allow prefetching, etc
    virtual void prepareForNextBlock (const AudioRenderContext&)   {}
    virtual bool isReadyToRender() = 0;

    virtual void renderOver (const AudioRenderContext&) = 0;
    virtual void renderAdding (const AudioRenderContext&) = 0;

    void callRenderAdding (const AudioRenderContext&);
    void callRenderOver (const AudioRenderContext&);

    //==============================================================================
    template <typename CallbackType>
    static void invokeSplitRender (const AudioRenderContext& rc, CallbackType& target)
    {
        const PlayHead::EditTimeWindow editTime (rc.getEditTime());

        if (editTime.isSplit)
        {
            AudioRenderContext rc2 (rc);

            auto firstLen = editTime.editRange1.getLength();
            auto firstSamps = juce::jmin (rc.bufferNumSamples,
                                          (int) (firstLen * rc.bufferNumSamples / rc.streamTime.getLength()));

            rc2.streamTime = rc2.streamTime.withLength (firstLen);
            rc2.bufferNumSamples = firstSamps;
            rc2.continuity |= AudioRenderContext::lastBlockBeforeLoop;

            target.renderSection (rc2, editTime.editRange1);

            rc2.streamTime = { rc2.streamTime.getEnd(), rc.streamTime.getEnd() };
            rc2.bufferStartSample += firstSamps;
            rc2.bufferNumSamples = rc.bufferNumSamples - firstSamps;
            rc2.midiBufferOffset += firstLen;
            rc2.continuity = AudioRenderContext::firstBlockOfLoop;

            target.renderSection (rc2, editTime.editRange2);
        }
        else if (rc.playhead.isLooping() && ! rc.playhead.isRollingIntoLoop())
        {
            // In the case where looping happens to line up exactly with the audio
            // blocks being rendered, set the proper continuity flags
            AudioRenderContext rc2(rc);

            auto loop = rc.playhead.getLoopTimes();

            auto s = editTime.editRange1.getStart();
            auto e = editTime.editRange1.getEnd();

            if (e >= loop.getEnd() - 0.000001)
                rc2.continuity |= AudioRenderContext::lastBlockBeforeLoop;
            if (s <= loop.getStart() + 0.000001)
                rc2.continuity = AudioRenderContext::firstBlockOfLoop;

            target.renderSection (rc2, editTime.editRange1);
        }
        else
        {
            target.renderSection (rc, editTime.editRange1);
        }
    }

private:
    void callRenderOverForMidi (const AudioRenderContext&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioNode)
};

//==============================================================================
class SingleInputAudioNode  : public AudioNode
{
public:
    SingleInputAudioNode (AudioNode* input);

    void getAudioNodeProperties (AudioNodeProperties&) override;
    void visitNodes (const VisitorFn&) override;
    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo&) override;
    juce::ReferenceCountedObjectPtr<Plugin> getPlugin() const override;
    bool isReadyToRender() override;
    bool purgeSubNodes (bool keepAudio, bool keepMidi) override;
    void releaseAudioNodeResources() override;
    void prepareForNextBlock (const AudioRenderContext&) override;
    void renderOver (const AudioRenderContext&) override;
    void renderAdding (const AudioRenderContext&) override;

    const std::unique_ptr<AudioNode> input;

private:
    SingleInputAudioNode() = delete;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SingleInputAudioNode)
};

//==============================================================================
struct MuteAudioNode  : public SingleInputAudioNode
{
    MuteAudioNode (AudioNode* input) : SingleInputAudioNode (input) {}

    void renderOver (const AudioRenderContext& rc) override
    {
        input->renderAdding (rc);
        rc.clearAll();
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        callRenderOver (rc);
    }
};

} // namespace tracktion_engine
