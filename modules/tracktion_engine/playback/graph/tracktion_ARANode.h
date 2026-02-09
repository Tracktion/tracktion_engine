/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
/**
    Node for playing back audio through an ARA plugin.

    This node handles playback of audio clips processed through ARA-compatible
    plugins such as Melodyne.
*/
class ARANode final  : public tracktion::graph::Node,
                       private juce::Timer
{
public:
    ARANode (AudioClipBase&, tracktion::graph::PlayHead&, bool isOfflineRender);
    ~ARANode() override;

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    class ARAPlayhead;

    AudioClipBase& clip;
    tracktion::graph::PlayHead& playHead;
    LiveClipLevel clipLevel;
    Clip::Ptr clipPtr;
    ARAFileReader::Ptr araProxy;
    const AudioFileInfo fileInfo;
    juce::MidiBuffer midiMessages;
    juce::PluginDescription desc;
    std::unique_ptr<ARAPlayhead> playhead;
    bool isOfflineRender = false;
    std::atomic<bool> analysingContent { true };

    //==============================================================================
    void updateAnalysingState();
    void timerCallback() override;
};

/** @deprecated Use ARANode instead */
using MelodyneNode = ARANode;

}} // namespace tracktion { inline namespace engine
