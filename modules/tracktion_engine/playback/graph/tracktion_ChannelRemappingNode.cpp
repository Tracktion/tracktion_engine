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
ChannelRemappingNode::ChannelRemappingNode (std::unique_ptr<tracktion::graph::Node> inputNode,
                                            ChannelMap channelMapToUse)
    : mode (Mode::ExplicitMapping),
      input (std::move (inputNode)),
      channelMap (std::move (channelMapToUse))
{
    jassert (input != nullptr);

    // Need to clear buffers since we use add() for accumulation
    setOptimisations ({ tracktion::graph::ClearBuffers::yes,
                        tracktion::graph::AllocateAudioBuffer::yes });
}

ChannelRemappingNode::ChannelRemappingNode (std::unique_ptr<tracktion::graph::Node> processorNode,
                                            ChannelConfiguration inputConfig,
                                            ChannelConfiguration processorConfig)
    : mode (Mode::ProcessorPassthrough),
      input (std::move (processorNode)),
      inputChannelConfig (std::move (inputConfig)),
      processorChannelConfig (std::move (processorConfig))
{
    jassert (input != nullptr);
    jassert (! inputChannelConfig.isEmpty());

    // An empty processorConfig means "pass-through: handles all channels",
    // so treat it as matching the input config
    if (processorChannelConfig.isEmpty())
        processorChannelConfig = inputChannelConfig;

    // Cache the original input node - it's the first direct input of the processor
    // We need this for passthrough mode to access channels beyond what the processor handles
    auto processorInputs = input->getDirectInputNodes();

    if (! processorInputs.empty())
        originalInputNode = processorInputs[0];

    setOptimisations ({ tracktion::graph::ClearBuffers::no,
                        tracktion::graph::AllocateAudioBuffer::yes });
}

//==============================================================================
tracktion::graph::NodeProperties ChannelRemappingNode::getNodeProperties()
{
    auto props = input->getNodeProperties();

    constexpr size_t channelRemappingMagicHash = size_t (0x6368616e72656d); // "chanrem"

    if (mode == Mode::ExplicitMapping)
    {
        props.hasAudio = ! channelMap.isEmpty();
        props.numberOfChannels = channelMap.getRequiredOutputChannels();

        // Include mapping in hash
        for (const auto& e : channelMap.entries)
        {
            hash_combine (props.nodeID, e.source);
            hash_combine (props.nodeID, e.dest);
        }
    }
    else // ProcessorPassthrough
    {
        const auto numInputChannels = inputChannelConfig.getNumChannels();
        const auto numProcessorChannels = processorChannelConfig.getNumChannels();

        // Output channel count depends on mode:
        // - Passthrough mode (input > processor): output = input channels
        // - Expansion mode (input < processor): output = processor channels
        props.numberOfChannels = std::max (numInputChannels, numProcessorChannels);

        hash_combine (props.nodeID, numInputChannels);
        hash_combine (props.nodeID, numProcessorChannels);
    }

    if (props.nodeID != 0)
        hash_combine (props.nodeID, channelRemappingMagicHash);

    return props;
}

std::vector<tracktion::graph::Node*> ChannelRemappingNode::getDirectInputNodes()
{
    return { input.get() };
}

bool ChannelRemappingNode::isReadyToProcess()
{
    return input->hasProcessed();
}

void ChannelRemappingNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo&)
{
    // Nothing special needed - input handles its own preparation
}

void ChannelRemappingNode::process (ProcessContext& pc)
{
    if (mode == Mode::ExplicitMapping)
        processExplicitMapping (pc);
    else
        processProcessorPassthrough (pc);
}

//==============================================================================
void ChannelRemappingNode::processExplicitMapping (ProcessContext& pc)
{
    auto inputBuffers = input->getProcessedOutput();

    // Always pass through MIDI
    pc.buffers.midi.copyFrom (inputBuffers.midi);

    // Remap audio channels - use add() to accumulate (allows multiple sources to one dest)
    for (const auto& e : channelMap.entries)
    {
        if (e.source < static_cast<int> (inputBuffers.audio.getNumChannels())
            && e.dest < static_cast<int> (pc.buffers.audio.getNumChannels()))
        {
            auto destChan = pc.buffers.audio.getChannel (static_cast<choc::buffer::ChannelCount> (e.dest));
            auto srcChan = inputBuffers.audio.getChannel (static_cast<choc::buffer::ChannelCount> (e.source));

            if (e.gain == 1.0f)
                add (destChan, srcChan);
            else
                add (destChan, srcChan, e.gain);
        }
    }
}

void ChannelRemappingNode::processProcessorPassthrough (ProcessContext& pc)
{
    auto& destAudio = pc.buffers.audio;
    auto& destMidi = pc.buffers.midi;

    const auto numInputChannels = inputChannelConfig.getNumChannels();
    const auto numProcessorChannels = processorChannelConfig.getNumChannels();

    // Get the processor's output
    auto processorOutput = input->getProcessedOutput();
    const auto numProcessorOutputChannels = static_cast<int> (processorOutput.audio.getNumChannels());

    // Copy MIDI from processor
    destMidi.copyFrom (processorOutput.midi);

    // Copy processed channels from processor output
    const auto numChannelsToCopyFromProcessor = std::min (numProcessorOutputChannels,
                                                          static_cast<int> (destAudio.getNumChannels()));

    if (numChannelsToCopyFromProcessor > 0)
    {
        auto destProcessedChannels = destAudio.getFirstChannels (static_cast<choc::buffer::ChannelCount> (numChannelsToCopyFromProcessor));
        auto srcProcessedChannels = processorOutput.audio.getFirstChannels (static_cast<choc::buffer::ChannelCount> (numChannelsToCopyFromProcessor));
        copy (destProcessedChannels, srcProcessedChannels);
    }

    // PASSTHROUGH MODE: Copy extra channels from original input that the processor didn't handle
    if (numInputChannels > numProcessorChannels && originalInputNode != nullptr)
    {
        auto originalInput = originalInputNode->getProcessedOutput();
        const auto numOriginalChannels = static_cast<int> (originalInput.audio.getNumChannels());
        const auto numPassthroughChannels = numInputChannels - numProcessorChannels;

        // Copy channels from processor channel count onwards
        for (int ch = 0; ch < numPassthroughChannels; ++ch)
        {
            const auto srcChannel = numProcessorChannels + ch;
            const auto destChannel = numProcessorChannels + ch;

            if (srcChannel < numOriginalChannels && destChannel < static_cast<int> (destAudio.getNumChannels()))
            {
                copy (destAudio.getChannel (static_cast<choc::buffer::ChannelCount> (destChannel)),
                      originalInput.audio.getChannel (static_cast<choc::buffer::ChannelCount> (srcChannel)));
            }
            else if (destChannel < static_cast<int> (destAudio.getNumChannels()))
            {
                // Source channel doesn't exist, fill with silence
                destAudio.getChannel (static_cast<choc::buffer::ChannelCount> (destChannel)).clear();
            }
        }
    }

    // EXPANSION MODE: The processor already outputs enough channels, nothing extra to do
    // (processor output was already copied above)
}

}} // namespace tracktion { inline namespace engine
