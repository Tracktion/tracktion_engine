/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


namespace tracktion { inline namespace engine
{

LevelMeasuringNode::LevelMeasuringNode (std::unique_ptr<tracktion::graph::Node> inputNode, LevelMeasurer& measurer)
    : input (std::move (inputNode)), levelMeasurer (measurer)
{
    setOptimisations ({ tracktion::graph::ClearBuffers::no,
                        tracktion::graph::AllocateAudioBuffer::no });
}

void LevelMeasuringNode::process (tracktion::graph::Node::ProcessContext& pc)
{
    auto sourceBuffers = input->getProcessedOutput();
    jassert (pc.buffers.audio.getSize() == sourceBuffers.audio.getSize());

    // Just pass out input on to our output
    setAudioOutput (input.get(), sourceBuffers.audio);
    
    // If the source only outputs to this node, we can steal its data
    if (numOutputNodes == 1)
        pc.buffers.midi.swapWith (sourceBuffers.midi);
    else
        pc.buffers.midi.copyFrom (sourceBuffers.midi);

    // Then update the levels
    if (sourceBuffers.audio.getNumChannels() > 0)
    {
        auto buffer = tracktion::graph::toAudioBuffer (sourceBuffers.audio);
        levelMeasurer.processBuffer (buffer, 0, buffer.getNumSamples());
    }

    levelMeasurer.processMidi (pc.buffers.midi, nullptr);
}

}} // namespace tracktion { inline namespace engine
