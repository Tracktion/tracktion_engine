/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion_graph
{

//==============================================================================
//==============================================================================
/** Creates a node of the given type and returns it as the base AudioNode class. */
template<typename AudioNodeType, typename... Args>
std::unique_ptr<AudioNode> makeAudioNode (Args&&... args)
{
    return std::unique_ptr<tracktion_graph::AudioNode> (std::move (std::make_unique<AudioNodeType> (std::forward<Args> (args)...)));
}


//==============================================================================
//==============================================================================
class LatencyAudioNode  : public AudioNode
{
public:
    LatencyAudioNode (std::unique_ptr<AudioNode> inputNode, int numSamplesToDelay)
        : LatencyAudioNode (inputNode.get(), numSamplesToDelay)
    {
        ownedInput = std::move (inputNode);
    }

    LatencyAudioNode (AudioNode* inputNode, int numSamplesToDelay)
        : input (inputNode), latencyNumSamples (numSamplesToDelay)
    {
    }

    AudioNodeProperties getAudioNodeProperties() override
    {
        auto props = input->getAudioNodeProperties();
        props.latencyNumSamples += latencyNumSamples;
        
        return props;
    }
    
    std::vector<AudioNode*> getDirectInputNodes() override
    {
        return { input };
    }

    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate;
        latencyTimeSeconds = latencyNumSamples / info.sampleRate;
        
        fifo.setSize (getAudioNodeProperties().numberOfChannels, latencyNumSamples + info.blockSize + 1);
        fifo.writeSilence (latencyNumSamples);
        jassert (fifo.getNumReady() == latencyNumSamples);
    }
    
    void process (const ProcessContext& pc) override
    {
        auto& outputBlock = pc.buffers.audio;
        auto inputBuffer = input->getProcessedOutput().audio;
        auto& inputMidi = input->getProcessedOutput().midi;
        const int numSamples = (int) pc.streamSampleRange.getLength();

        if (fifo.getNumChannels() > 0)
        {
            jassert (numSamples == (int) outputBlock.getNumSamples());
            jassert (fifo.getNumChannels() == (int) inputBuffer.getNumChannels());
            
            // Write to audio delay buffer
            fifo.write (inputBuffer);

            // Then read from them
            jassert (fifo.getNumReady() >= (int) outputBlock.getNumSamples());
            fifo.readAdding (outputBlock);
        }

        // Then write to MIDI delay buffer
        midi.mergeFromWithOffset (inputMidi, latencyTimeSeconds);


        // And read out any delayed items
        const double blockTimeSeconds = numSamples / sampleRate;
     
        for (int i = midi.size(); --i >= 0;)
        {
            auto& m = midi[i];
            
            if (m.getTimeStamp() <= blockTimeSeconds)
            {
                pc.buffers.midi.add (m);
                midi.remove (i);
                // TODO: This will deallocate, we need a MIDI message array that doesn't adjust its storage
            }
        }
        
        // Shuffle down remaining items by block time
        midi.addToTimestamps (-blockTimeSeconds);
        
        // Ensure there are no negative time messages
        for (auto& m : midi)
        {
            juce::ignoreUnused (m);
            jassert (m.getTimeStamp() >= 0.0);
        }
    }
    
private:
    std::unique_ptr<AudioNode> ownedInput;
    AudioNode* input;
    const int latencyNumSamples;
    double sampleRate = 44100.0;
    double latencyTimeSeconds = 0.0;
    AudioFifo fifo { 1, 32 };
    tracktion_engine::MidiMessageArray midi;
};


//==============================================================================
//==============================================================================
/**
    An AudioNode which sums together the multiple inputs adding additional latency
    to provide a coherent output.
*/
class SummingAudioNode : public AudioNode
{
public:
    SummingAudioNode (std::vector<std::unique_ptr<AudioNode>> inputs)
        : ownedNodes (std::move (inputs))
    {
        for (auto& ownedNode : ownedNodes)
            nodes.push_back (ownedNode.get());
    }

    SummingAudioNode (std::vector<AudioNode*> inputs)
        : nodes (std::move (inputs))
    {
    }
    
    SummingAudioNode (std::vector<std::unique_ptr<AudioNode>> ownedInputs,
                      std::vector<AudioNode*> referencedInputs)
        : SummingAudioNode (std::move (ownedInputs))
    {
        nodes.insert (nodes.begin(), referencedInputs.begin(), referencedInputs.end());
    }

    AudioNodeProperties getAudioNodeProperties() override
    {
        AudioNodeProperties props;
        props.hasAudio = false;
        props.hasMidi = false;
        props.numberOfChannels = 0;

        for (auto& node : nodes)
        {
            auto nodeProps = node->getAudioNodeProperties();
            props.hasAudio = props.hasAudio | nodeProps.hasAudio;
            props.hasMidi = props.hasMidi | nodeProps.hasMidi;
            props.numberOfChannels = std::max (props.numberOfChannels, nodeProps.numberOfChannels);
            props.latencyNumSamples = std::max (props.latencyNumSamples, nodeProps.latencyNumSamples);
        }

        return props;
    }
    
    std::vector<AudioNode*> getDirectInputNodes() override
    {
        std::vector<AudioNode*> inputNodes;
        
        for (auto& node : nodes)
            inputNodes.push_back (node);

        return inputNodes;
    }

    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        createLatencyNodes (info);
    }

    bool isReadyToProcess() override
    {
        for (auto& node : nodes)
            if (! node->hasProcessed())
                return false;
        
        return true;
    }
    
    void process (const ProcessContext& pc) override
    {
        const auto numChannels = pc.buffers.audio.getNumChannels();

        // Get each of the inputs and add them to dest
        for (auto& node : nodes)
        {
            auto inputFromNode = node->getProcessedOutput();
            
            const auto numChannelsToAdd = std::min (inputFromNode.audio.getNumChannels(), numChannels);

            if (numChannelsToAdd > 0)
                pc.buffers.audio.getSubsetChannelBlock (0, numChannelsToAdd)
                    .add (node->getProcessedOutput().audio.getSubsetChannelBlock (0, numChannelsToAdd));
            
            pc.buffers.midi.mergeFrom (inputFromNode.midi);
        }
    }

private:
    std::vector<std::unique_ptr<AudioNode>> ownedNodes;
    std::vector<AudioNode*> nodes;
    
    void createLatencyNodes (const PlaybackInitialisationInfo& info)
    {
        const int maxLatency = getAudioNodeProperties().latencyNumSamples;
        std::vector<std::unique_ptr<AudioNode>> ownedNodesToAdd;

        for (auto& node : nodes)
        {
            const int nodeLatency = node->getAudioNodeProperties().latencyNumSamples;
            const int latencyToAdd = maxLatency - nodeLatency;
            
            if (latencyToAdd == 0)
                continue;
            
            auto getOwnedNode = [this] (auto nodeToFind)
            {
                for (auto& ownedNode : ownedNodes)
                {
                    if (ownedNode.get() == nodeToFind)
                    {
                        auto nodeToReturn = std::move (ownedNode);
                        ownedNode.reset();
                        return nodeToReturn;
                    }
                }
                
                return std::unique_ptr<AudioNode>();
            };
            
            auto ownedNode = getOwnedNode (node);
            auto latencyNode = ownedNode != nullptr ? makeAudioNode<LatencyAudioNode> (std::move (ownedNode), latencyToAdd)
                                                    : makeAudioNode<LatencyAudioNode> (node, latencyToAdd);
            latencyNode->initialise (info);
            ownedNodesToAdd.push_back (std::move (latencyNode));
            node = nullptr;
        }
        
        // Take ownership of any new nodes and also ensure they're reference in the raw array
        for (auto& newNode : ownedNodesToAdd)
        {
            nodes.push_back (newNode.get());
            ownedNodes.push_back (std::move (newNode));
        }

        nodes.erase (std::remove_if (nodes.begin(), nodes.end(),
                                     [] (auto& n) { return n == nullptr; }),
                     nodes.end());

        ownedNodes.erase (std::remove_if (ownedNodes.begin(), ownedNodes.end(),
                                          [] (auto& n) { return n == nullptr; }),
                          ownedNodes.end());
    }
};


/** Creates a SummingAudioNode from a number of AudioNodes. */
static inline std::unique_ptr<SummingAudioNode> makeSummingAudioNode (std::initializer_list<AudioNode*> nodes)
{
    std::vector<std::unique_ptr<AudioNode>> nodeVector;
    
    for (auto node : nodes)
        nodeVector.push_back (std::unique_ptr<AudioNode> (node));
        
    return std::make_unique<SummingAudioNode> (std::move (nodeVector));
}

}
