/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion::inline graph {

//==============================================================================
//==============================================================================
class LatencyNode final : public Node
{
public:
    LatencyNode (std::unique_ptr<Node> inputNode, int numSamplesToDelay)
        : LatencyNode (inputNode.get(), numSamplesToDelay)
    {
        ownedInput = std::move (inputNode);

        setOptimisations ({ .clear = tracktion::graph::ClearBuffers::no,
                            .allocate = tracktion::graph::AllocateAudioBuffer::yes,
                            .prefetch = tracktion::graph::PrefetchBlock::no });
    }

    LatencyNode (std::shared_ptr<Node> inputNode, int numSamplesToDelay)
        : LatencyNode (inputNode.get(), numSamplesToDelay)
    {
        sharedInput = std::move (inputNode);

        setOptimisations ({ .clear = tracktion::graph::ClearBuffers::no,
                            .allocate = tracktion::graph::AllocateAudioBuffer::yes,
                            .prefetch = tracktion::graph::PrefetchBlock::no });
    }

    LatencyNode (Node* inputNode, int numSamplesToDelay)
        : input (inputNode)
    {
        latencyProcessor->setLatencyNumSamples (numSamplesToDelay);

        setOptimisations ({ .clear = tracktion::graph::ClearBuffers::no,
                            .allocate = tracktion::graph::AllocateAudioBuffer::yes,
                            .prefetch = tracktion::graph::PrefetchBlock::no });
    }

    NodeProperties getNodeProperties() override
    {
        auto props = input->getNodeProperties();
        props.latencyNumSamples += latencyProcessor->getLatencyNumSamples();

        constexpr size_t latencyNodeMagicHash = size_t (0x95ab5e9dcc);

        if (props.nodeID != 0)
            hash_combine (props.nodeID, latencyNodeMagicHash);

        return props;
    }

    std::vector<Node*> getDirectInputNodes() override
    {
        return { input };
    }

    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }

    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        const auto numChannels = getNodeProperties().numberOfChannels;
        latencyProcessor->prepareToPlay (info.sampleRate, info.blockSize, numChannels);
        replaceLatencyProcessorIfPossible (info, numChannels);

        if (previousProcessor)
        {
            numFadeFrames = std::max (1, (int) timeToSample (0.01, info.sampleRate));
            previousOutput.resize ({ (choc::buffer::ChannelCount) numChannels,
                                     (choc::buffer::FrameCount) info.blockSize });
        }
    }

    void process (ProcessContext& pc) override
    {
        auto inputBuffer = input->getProcessedOutput().audio;
        auto& inputMidi = input->getProcessedOutput().midi;
        auto numSamples = (int) pc.referenceSampleRange.getLength();
        jassert (pc.buffers.audio.getNumChannels() == 0 || numSamples == (int) pc.buffers.audio.getNumFrames());

        latencyProcessor->writeAudio (inputBuffer);
        latencyProcessor->writeMIDI (inputMidi);

        pc.buffers.midi.clear();
        latencyProcessor->readAudioOverwriting (pc.buffers.audio);
        latencyProcessor->readMIDI (pc.buffers.midi, numSamples);

        if (previousProcessor && numFramesFaded < latencyProcessor->getLatencyNumSamples() + numFadeFrames)
            crossfadeFromPreviousProcessor (inputBuffer, pc.buffers.audio);
    }

private:
    std::unique_ptr<Node> ownedInput;
    std::shared_ptr<Node> sharedInput;
    Node* input = nullptr;
    std::shared_ptr<LatencyProcessor> latencyProcessor { std::make_shared<LatencyProcessor>() };

    // The delay of the LatencyNode this replaced, if only its latency was different
    std::shared_ptr<LatencyProcessor> previousProcessor;
    choc::buffer::ChannelArrayBuffer<float> previousOutput;
    int numFadeFrames = 0, numFramesFaded = 0;

    void replaceLatencyProcessorIfPossible (const PlaybackInitialisationInfo& info, int numChannels)
    {
        if (info.nodeGraphToReplace == nullptr)
            return;

        const auto nodeIDToLookFor = getNodeProperties().nodeID;

        if (nodeIDToLookFor == 0)
            return;

        if (auto oldNode = findNodeWithID<LatencyNode> (*info.nodeGraphToReplace, nodeIDToLookFor))
        {
            auto& old = oldNode->latencyProcessor;

            if (latencyProcessor->hasSameConfigurationAs (*old))
                latencyProcessor = old;
            else if (old->hasConfiguration (old->getLatencyNumSamples(), info.sampleRate, numChannels))
                previousProcessor = old;
        }
    }

    /** Carries on the delay this replaced alongside the new one for a few ms, fading it out, and
        fades the new one in from when its audio arrives, so the change in delay doesn't click.
    */
    void crossfadeFromPreviousProcessor (choc::buffer::ChannelArrayView<float> inputAudio,
                                         choc::buffer::ChannelArrayView<float> output)
    {
        const auto numFrames = output.getNumFrames();
        const auto newLatency = latencyProcessor->getLatencyNumSamples();
        const bool previousIsAudible = numFramesFaded < numFadeFrames && numFrames <= previousOutput.getNumFrames();
        auto previous = previousOutput.getStart (std::min (numFrames, previousOutput.getNumFrames()));

        if (previousIsAudible)
        {
            previousProcessor->writeAudio (inputAudio);
            previousProcessor->readAudioOverwriting (previous);
        }

        for (choc::buffer::FrameCount frame = 0; frame < numFrames; ++frame, ++numFramesFaded)
        {
            const auto fadeIn  = std::clamp ((float) (numFramesFaded - newLatency) / (float) numFadeFrames, 0.0f, 1.0f);
            const auto fadeOut = std::clamp (1.0f - (float) numFramesFaded / (float) numFadeFrames, 0.0f, 1.0f);

            for (choc::buffer::ChannelCount channel = 0; channel < output.getNumChannels(); ++channel)
                output.getSample (channel, frame) = output.getSample (channel, frame) * fadeIn
                                                     + (previousIsAudible ? previous.getSample (channel, frame) * fadeOut : 0.0f);
        }
    }
};

}
