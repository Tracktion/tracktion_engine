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

/**
    A Node that intercepts MIDI from a plugin callback and inserts it in to the playback graph.
*/
class HostedMidiInputDeviceNode final : public tracktion::graph::Node,
                                        public InputDeviceInstance::Consumer,
                                        private TracktionEngineNode
{
public:
    HostedMidiInputDeviceNode (InputDeviceInstance&,
                               MidiInputDevice&, MidiMessageArray::MPESourceID,
                               tracktion::graph::PlayHeadState&,
                               tracktion::ProcessState&);
    ~HostedMidiInputDeviceNode() override;

    tracktion::graph::NodeProperties getNodeProperties() override;
    void prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&) override;
    bool isReadyToProcess() override;
    void process (ProcessContext&) override;

    void handleIncomingMidiMessage (const juce::MidiMessage&) override;

private:
    //==============================================================================
    InputDeviceInstance& instance;
    const MidiMessageArray::MPESourceID midiSourceID = MidiMessageArray::notMPE;

    tracktion::graph::RealTimeSpinLock bufferMutex;
    MidiMessageArray incomingMessages;
    double sampleRate = 44100.0;
};

}} // namespace tracktion { inline namespace engine
