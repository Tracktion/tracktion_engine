/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/** An AudioNode that synchronises its input node with the playback position.

    Nightmare to try to explain this one - it basically handles looping, etc. by
    converting linear output times into edit times and passing those through
    to its input node.
*/
class PlayHeadAudioNode  : public SingleInputAudioNode
{
public:
    PlayHeadAudioNode (AudioNode*);
    ~PlayHeadAudioNode();

    //==============================================================================
    void getAudioNodeProperties (AudioNodeProperties&) override;
    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo&) override;

    void renderOver (const AudioRenderContext&) override;
    void renderAdding (const AudioRenderContext&) override;

private:
    //==============================================================================
    juce::Time lastUserInteractionTime;
    bool hasAnyContent = false, isPlayHeadRunning = false, firstCallback = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayHeadAudioNode)
};

} // namespace tracktion_engine
