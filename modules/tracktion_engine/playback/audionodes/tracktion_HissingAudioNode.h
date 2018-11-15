/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** An AudioNode that adds the 'unregistered' hiss noise to the output. */
class HissingAudioNode  : public SingleInputAudioNode
{
public:
    //==============================================================================
    HissingAudioNode (AudioNode*);
    ~HissingAudioNode();

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo&) override;

    void renderOver (const AudioRenderContext&) override;
    void renderAdding (const AudioRenderContext&) override;

private:
    int blockCounter = 0, blocksBetweenHisses = 0, maxBlocksBetweenHisses = 0, blocksPerHiss = 0;
    double level = 0;
    juce::Random random;
};

} // namespace tracktion_engine
