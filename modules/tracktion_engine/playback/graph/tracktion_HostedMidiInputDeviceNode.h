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

/**
    A Node that intercepts MIDI from a plugin callback and inserts it in to the playback graph.
*/
class HostedMidiInputDeviceNode final : public tracktion::graph::Node,
                                        public InputDeviceInstance::Consumer,
                                        private TracktionEngineNode
{
public:
    HostedMidiInputDeviceNode (InputDeviceInstance&,
                               MidiInputDevice&,
                               tracktion::graph::PlayHeadState&,
                               tracktion::ProcessState&);
    ~HostedMidiInputDeviceNode() override;

    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

    void handleIncomingMidiMessage (const juce::MidiMessage&, MPESourceID) override;

private:
    //==============================================================================
    InputDeviceInstance& instance;

    tracktion::graph::RealTimeSpinLock bufferMutex;
    MidiMessageArray incomingMessages;
    double sampleRate = 44100.0;
};

}} // namespace tracktion { inline namespace engine
