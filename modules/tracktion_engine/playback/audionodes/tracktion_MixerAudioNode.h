/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** An AudioNode that mixes together a set of parallel input nodes. */
class MixerAudioNode   : public AudioNode
{
public:
    MixerAudioNode (bool shouldUse64Bit, bool multiCpuMode);

    static void updateNumCPUs (Engine&);

    //==============================================================================
    /** Adds an input node.
        This will be deleted by this node when no longer needed.
    */
    void addInput (AudioNode*);

    /** deletes all the input nodes */
    void clear();

    //==============================================================================
    // AudioNode methods..

    void getAudioNodeProperties (AudioNodeProperties&) override;
    void visitNodes (const VisitorFn&) override;

    bool purgeSubNodes (bool keepAudio, bool keepMidi) override;

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo&) override;
    bool isReadyToRender() override;
    void releaseAudioNodeResources() override;

    void renderOver (const AudioRenderContext&) override;
    void renderAdding (const AudioRenderContext&) override;
    void prepareForNextBlock (const AudioRenderContext&) override;

private:
    void multiCpuRender (const AudioRenderContext&);

    //==============================================================================
    juce::OwnedArray<AudioNode> inputs;

    bool hasAudio = false, hasMidi = false;
    int maxNumberOfChannels = 0;

    juce::AudioBuffer<double> temp64bitBuffer;
    const bool use64bitMixing;
    const bool canUseMultiCpu;
    bool shouldUseMultiCpu = false;

    void set64bitBufferSize (int samples, int numChans);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerAudioNode)
};

} // namespace tracktion_engine
