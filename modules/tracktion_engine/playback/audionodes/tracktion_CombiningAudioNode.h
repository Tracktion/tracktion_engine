/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** An AudioNode that mixes a sequence of clips of other nodes.

    This node takes a set of input AudioNodes with associated start + end times,
    and mixes together their output.

    It initialises and releases its inputs as required according to its current
    play position.
*/
class CombiningAudioNode   : public AudioNode
{
public:
    CombiningAudioNode();
    ~CombiningAudioNode();

    //==============================================================================
    /** Adds an input node to be played.

        The offset is relative to the combining node's zero-time, so the input node's
        time of 0 is equal to its (start + offset) relative to the combiner node's start.

        Any nodes passed-in will be deleted by this node when required.
    */
    void addInput (EditTimeRange time, AudioNode* inputNode);

    void clear();

    //==============================================================================
    void getAudioNodeProperties (AudioNodeProperties&) override;
    void visitNodes (const VisitorFn&) override;

    bool purgeSubNodes (bool keepAudio, bool keepMidi) override;

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo&) override;
    bool isReadyToRender() override;
    void releaseAudioNodeResources() override;

    void renderOver (const AudioRenderContext&) override;
    void renderAdding (const AudioRenderContext&) override;
    void prepareForNextBlock (const AudioRenderContext&) override;

    void renderSection (const AudioRenderContext&, EditTimeRange editTime);

private:
    struct TimedAudioNode;
    juce::OwnedArray<TimedAudioNode> inputs;
    juce::OwnedArray<juce::Array<TimedAudioNode*>> groups;

    bool hasAudio = false, hasMidi = false;
    int maxNumberOfChannels = 0;

    void prefetchGroup (const AudioRenderContext&, double time);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CombiningAudioNode)
};

} // namespace tracktion_engine
