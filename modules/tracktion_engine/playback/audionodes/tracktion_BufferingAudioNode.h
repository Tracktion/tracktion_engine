/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/**
    AudioNode that buffers its input at a larger buffer size than the calling AudioNode
    to reduce the ammount of function calls during a render pass.
*/
class BufferingAudioNode    : public SingleInputAudioNode
{
public:
    /** Creates a BufferingAudioNode for a given input AudioNode.
        The bufferSize provided here is just a hint, it assures that the buffer sized used
        will be at least that big but for speed and to avoid using circular buffers the
        actual buffer size will always be a multiple of the input size requested.
    */
    BufferingAudioNode (AudioNode* input, int minBufferSize);

    /** Destructor. */
    ~BufferingAudioNode();

    void getAudioNodeProperties (AudioNodeProperties& info) override;
    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override;

    void prepareForNextBlock (const AudioRenderContext& rc) override;
    void renderOver (const AudioRenderContext& rc) override;
    void renderAdding (const AudioRenderContext& rc) override;

private:
    //==============================================================================
    int bufferSize, numSamplesLeft;
    double sampleRate;
    juce::AudioBuffer<float> tempBuffer;
    MidiMessageArray tempMidiBuffer;

    AudioRenderContext getTempContext (const AudioRenderContext& rc);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BufferingAudioNode)
};

} // namespace tracktion_engine
