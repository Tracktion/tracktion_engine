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
    A Node that calls the listeners of an AudioTrack with any incomming MIDI.
*/
class LiveMidiOutputNode final : public tracktion::graph::Node,
                                 private juce::AsyncUpdater
{
public:
    LiveMidiOutputNode (AudioTrack&, std::unique_ptr<tracktion::graph::Node>);
    LiveMidiOutputNode (Clip&, std::unique_ptr<tracktion::graph::Node>);

    //==============================================================================
    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<Node*> getDirectInputNodes() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

private:
    //==============================================================================
    AudioTrack* track = nullptr;
    Track::Ptr trackPtr;

    Clip::Ptr clipPtr;

    std::unique_ptr<tracktion::graph::Node> input;

    juce::CriticalSection lock;
    MidiMessageArray pendingMessages, dispatchingMessages;

    //==============================================================================
    void handleAsyncUpdate() override;
};

}} // namespace tracktion { inline namespace engine
