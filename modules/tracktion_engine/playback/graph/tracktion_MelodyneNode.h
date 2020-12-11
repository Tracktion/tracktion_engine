/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

//==============================================================================
//==============================================================================
/**
    Plays back a Melodyne plugin.
*/
class MelodyneNode final  : public tracktion_graph::Node,
                            private juce::Timer
{
public:
    MelodyneNode (AudioClipBase&, tracktion_graph::PlayHead&, bool isOfflineRender);
    ~MelodyneNode() override;

    //==============================================================================
    tracktion_graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion_graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    class MelodynePlayhead;
    
    AudioClipBase& clip;
    tracktion_graph::PlayHead& playHead;
    LiveClipLevel clipLevel;
    Clip::Ptr clipPtr;
    MelodyneFileReader::Ptr melodyneProxy;
    const AudioFileInfo fileInfo;
    juce::MidiBuffer midiMessages;
    juce::PluginDescription desc;
    std::unique_ptr<MelodynePlayhead> playhead;
    bool isOfflineRender = false;
    std::atomic<bool> analysingContent { true };
    
    //==============================================================================
    void updateAnalysingState();
    void timerCallback() override;
};

} // namespace tracktion_engine
