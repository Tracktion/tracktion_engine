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

LevelMeasuringNode::LevelMeasuringNode (std::unique_ptr<tracktion_graph::Node> inputNode, LevelMeasurer& measurer)
    : input (std::move (inputNode)), levelMeasurer (measurer)
{
}

void LevelMeasuringNode::process (tracktion_graph::Node::ProcessContext& pc)
{
    auto sourceBuffers = input->getProcessedOutput();
    jassert (pc.buffers.audio.getSize() == sourceBuffers.audio.getSize());

    // Just pass out input on to our output
    setAudioOutput (sourceBuffers.audio);
    pc.buffers.midi.mergeFrom (input->getProcessedOutput().midi);

    // Then update the levels
    auto buffer = tracktion_graph::createAudioBuffer (sourceBuffers.audio);
    levelMeasurer.processBuffer (buffer, 0, buffer.getNumSamples());
    levelMeasurer.processMidi (pc.buffers.midi, nullptr);
}

}
