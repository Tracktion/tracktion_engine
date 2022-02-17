/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace graph
{

//==============================================================================
//==============================================================================
class LatencyNode final : public Node
{
public:
    LatencyNode (std::unique_ptr<Node> inputNode, int numSamplesToDelay)
        : LatencyNode (inputNode.get(), numSamplesToDelay)
    {
        ownedInput = std::move (inputNode);

        setOptimisations ({ tracktion::graph::ClearBuffers::no,
                            tracktion::graph::AllocateAudioBuffer::yes });
    }

    LatencyNode (std::shared_ptr<Node> inputNode, int numSamplesToDelay)
        : LatencyNode (inputNode.get(), numSamplesToDelay)
    {
        sharedInput = std::move (inputNode);

        setOptimisations ({ tracktion::graph::ClearBuffers::no,
                            tracktion::graph::AllocateAudioBuffer::yes });
    }

    LatencyNode (Node* inputNode, int numSamplesToDelay)
        : input (inputNode)
    {
        latencyProcessor->setLatencyNumSamples (numSamplesToDelay);
        
        setOptimisations ({ tracktion::graph::ClearBuffers::no,
                            tracktion::graph::AllocateAudioBuffer::yes });
    }

    NodeProperties getNodeProperties() override
    {
        auto props = input->getNodeProperties();
        props.latencyNumSamples += latencyProcessor->getLatencyNumSamples();
        
        constexpr size_t latencyNodeMagicHash = 0x95ab5e9dcc;
        
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
        latencyProcessor->prepareToPlay (info.sampleRate, info.blockSize, getNodeProperties().numberOfChannels);
        replaceLatencyProcessorIfPossible (info.rootNodeToReplace);
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
    }
    
private:
    std::unique_ptr<Node> ownedInput;
    std::shared_ptr<Node> sharedInput;
    Node* input = nullptr;
    std::shared_ptr<LatencyProcessor> latencyProcessor { std::make_shared<LatencyProcessor>() };
    
    void replaceLatencyProcessorIfPossible (Node* rootNodeToReplace)
    {
        if (rootNodeToReplace == nullptr)
            return;
        
        auto nodeIDToLookFor = getNodeProperties().nodeID;
        
        if (nodeIDToLookFor == 0)
            return;

        auto visitor = [this, nodeIDToLookFor] (Node& node)
        {
            if (auto other = dynamic_cast<LatencyNode*> (&node))
            {
                if (other->getNodeProperties().nodeID == nodeIDToLookFor
                    && latencyProcessor->hasSameConfigurationAs (*other->latencyProcessor))
                {
                    latencyProcessor = other->latencyProcessor;
                }
            }
        };
        visitNodes (*rootNodeToReplace, visitor, true);
    }
};

}}
