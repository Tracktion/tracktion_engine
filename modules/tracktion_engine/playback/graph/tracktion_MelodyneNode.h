/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    You may use this code under the terms of the GPL v3 - see LICENCE.md for details.
    For the technical preview this file cannot be licensed commercially.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
/**
    Plays back a Melodyne plugin.
*/
class MelodyneNode final  : public tracktion::graph::Node,
                            private juce::Timer
{
public:
    MelodyneNode (AudioClipBase&, tracktion::graph::PlayHead&, bool isOfflineRender);
    ~MelodyneNode() override;

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    class MelodynePlayhead;

    AudioClipBase& clip;
    tracktion::graph::PlayHead& playHead;
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

}} // namespace tracktion { inline namespace engine
