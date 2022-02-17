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

SharedLevelMeasuringNode::SharedLevelMeasuringNode (SharedLevelMeasurer::Ptr source, std::unique_ptr<Node> inputNode)
    : levelMeasurer (std::move (source)), input (std::move (inputNode))
{
    jassert (levelMeasurer != nullptr);

    setOptimisations ({ tracktion::graph::ClearBuffers::no,
                        tracktion::graph::AllocateAudioBuffer::no });
}

std::vector<tracktion::graph::Node*> SharedLevelMeasuringNode::getDirectInputNodes()
{
    return { input.get() };
}

tracktion::graph::NodeProperties SharedLevelMeasuringNode::getNodeProperties()
{
    return input->getNodeProperties();
}

void SharedLevelMeasuringNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    levelMeasurer->setSize (2, info.blockSize);
}

bool SharedLevelMeasuringNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void SharedLevelMeasuringNode::prefetchBlock (juce::Range<int64_t> referenceSampleRange)
{
    levelMeasurer->startNextBlock (tracktion::graph::sampleToTime (referenceSampleRange.getStart(), sampleRate));
}

void SharedLevelMeasuringNode::process (ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK

    // Pass on input to output
    auto sourceBuffers = input->getProcessedOutput();

    setAudioOutput (input.get(), sourceBuffers.audio);
    pc.buffers.midi.copyFrom (sourceBuffers.midi);

    // And pass audio to level measurer
    auto buffer = tracktion::graph::toAudioBuffer (sourceBuffers.audio);
    levelMeasurer->addBuffer (buffer, 0, buffer.getNumSamples());
}

}} // namespace tracktion { inline namespace engine
