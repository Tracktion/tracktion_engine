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
/**
    Plays back a MIDI sequence.
    This simply plays it back from start to finish with no notation of a playhead.
*/
class MidiNode : public Node
{
public:
    MidiNode (juce::MidiMessageSequence sequenceToPlay)
        : sequence (std::move (sequenceToPlay))
    {
    }
    
    NodeProperties getNodeProperties() override
    {
        return { false, true, 0 };
    }
    
    bool isReadyToProcess() override
    {
        return true;
    }
    
    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        sampleRate = info.sampleRate;
    }
    
    void process (const ProcessContext& pc) override
    {
        const int numSamples = (int) pc.streamSampleRange.getLength();
        const double blockDuration = numSamples / sampleRate;
        const auto timeRange = juce::Range<double>::withStartAndLength (lastTime, blockDuration);
        
        for (int index = sequence.getNextIndexAtTime (timeRange.getStart()); index >= 0; ++index)
        {
            if (auto eventHolder = sequence.getEventPointer (index))
            {
                auto time = sequence.getEventTime (index);
                
                if (! timeRange.contains (time))
                    break;
                
                pc.buffers.midi.addMidiMessage (eventHolder->message, time - timeRange.getStart(),
                                                tracktion_engine::MidiMessageArray::notMPE);
            }
            else
            {
                break;
            }
        }
        
        lastTime = timeRange.getEnd();
    }
    
private:
    juce::MidiMessageSequence sequence;
    double sampleRate = 0.0, lastTime = 0.0;
};


//==============================================================================
//==============================================================================
class SinNode : public Node
{
public:
    SinNode (float frequency, int numChannelsToUse = 1, size_t nodeIDToUse = 0)
        : numChannels (numChannelsToUse), nodeID (nodeIDToUse)
    {
        osc.setFrequency (frequency, true);
    }
    
    NodeProperties getNodeProperties() override
    {
        NodeProperties props;
        props.hasAudio = true;
        props.hasMidi = false;
        props.numberOfChannels = numChannels;
        props.nodeID = nodeID;
        
        return props;
    }
    
    bool isReadyToProcess() override
    {
        return true;
    }
    
    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        osc.prepare ({ double (info.sampleRate), uint32_t (info.blockSize), (uint32_t) numChannels });
    }
    
    void process (const ProcessContext& pc) override
    {
        auto block = pc.buffers.audio;
        osc.process (juce::dsp::ProcessContextReplacing<float> { block });
        jassert (pc.buffers.audio.getNumChannels() == (size_t) getNodeProperties().numberOfChannels);
    }
    
private:
    juce::dsp::Oscillator<float> osc { [] (float in) { return std::sin (in); } };
    const int numChannels;
    size_t nodeID = 0;
};


//==============================================================================
//==============================================================================
/**
    Just a simple audio node that doesn't take any input so can be used as a stub.
*/
class SilentNode   : public Node
{
public:
    SilentNode (int numChannelsToUse = 1)
        : numChannels (numChannelsToUse)
    {
    }
    
    NodeProperties getNodeProperties() override
    {
        NodeProperties props;
        props.hasAudio = true;
        props.hasMidi = false;
        props.numberOfChannels = numChannels;
        
        return props;
    }
    
    bool isReadyToProcess() override
    {
        return true;
    }
    
    void prepareToPlay (const PlaybackInitialisationInfo&) override
    {
    }
    
    void process (const ProcessContext&) override
    {
    }
    
private:
    const int numChannels;
};


//==============================================================================
//==============================================================================
class BasicSummingNode : public Node
{
public:
    BasicSummingNode (std::vector<std::unique_ptr<Node>> inputs)
        : nodes (std::move (inputs))
    {
    }
    
    NodeProperties getNodeProperties() override
    {
        NodeProperties props;
        props.hasAudio = false;
        props.hasMidi = false;
        props.numberOfChannels = 0;

        for (auto& node : nodes)
        {
            auto nodeProps = node->getNodeProperties();
            props.hasAudio = props.hasAudio | nodeProps.hasAudio;
            props.hasMidi = props.hasMidi | nodeProps.hasMidi;
            props.numberOfChannels = std::max (props.numberOfChannels, nodeProps.numberOfChannels);
        }

        return props;
    }
    
    std::vector<Node*> getDirectInputNodes() override
    {
        std::vector<Node*> inputNodes;
        
        for (auto& node : nodes)
            inputNodes.push_back (node.get());

        return inputNodes;
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
    std::vector<std::unique_ptr<Node>> nodes;
};


/** Creates a SummingNode from a number of Nodes. */
static inline std::unique_ptr<BasicSummingNode> makeBaicSummingNode (std::initializer_list<Node*> nodes)
{
    std::vector<std::unique_ptr<Node>> nodeVector;
    
    for (auto node : nodes)
        nodeVector.push_back (std::unique_ptr<Node> (node));
        
    return std::make_unique<BasicSummingNode> (std::move (nodeVector));
}


//==============================================================================
//==============================================================================
class FunctionNode : public Node
{
public:
    FunctionNode (std::unique_ptr<Node> input,
                  std::function<float (float)> fn)
        : node (std::move (input)),
          function (std::move (fn))
    {
        jassert (function);
    }
    
    NodeProperties getNodeProperties() override
    {
        return node->getNodeProperties();
    }
    
    std::vector<Node*> getDirectInputNodes() override
    {
        return { node.get() };
    }

    bool isReadyToProcess() override
    {
        return node->hasProcessed();
    }
    
    void process (const ProcessContext& pc) override
    {
        auto inputBuffer = node->getProcessedOutput().audio;

        const int numSamples = (int) pc.streamSampleRange.getLength();
        const int numChannels = std::min ((int) inputBuffer.getNumChannels(), (int) pc.buffers.audio.getNumChannels());
        jassert ((int) inputBuffer.getNumSamples() == numSamples);

        for (int c = 0; c < numChannels; ++c)
        {
            const float* inputSamples = inputBuffer.getChannelPointer ((size_t) c);
            float* outputSamples = pc.buffers.audio.getChannelPointer ((size_t) c);
            
            for (int i = 0; i < numSamples; ++i)
                outputSamples[i] = function (inputSamples[i]);
        }
    }
    
private:
    std::unique_ptr<Node> node;
    std::function<float (float)> function;
};

//==============================================================================
/** Returns an audio node that applies a fixed gain to an input node. */
static inline std::unique_ptr<Node> makeGainNode (std::unique_ptr<Node> input, float gain)
{
    return makeNode<FunctionNode> (std::move (input), [gain] (float s) { return s * gain; });
}

//==============================================================================
//==============================================================================
class GainNode  : public Node
{
public:
    GainNode (Node* inputNode, std::function<float()> gainFunc)
        : input (inputNode), gainFunction (std::move (gainFunc))
    {
        assert (input != nullptr);
        assert (gainFunction);
        lastGain = gainFunction();
    }
    
    NodeProperties getNodeProperties() override
    {
        return input->getNodeProperties();
    }
    
    std::vector<Node*> getDirectInputNodes() override
    {
        return { input };
    }

    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void process (const ProcessContext& pc) override
    {
        jassert (pc.buffers.audio.getNumChannels() == input->getProcessedOutput().audio.getNumChannels());

        // Just pass out input on to our output
        pc.buffers.audio.copyFrom (input->getProcessedOutput().audio);
        pc.buffers.midi.mergeFrom (input->getProcessedOutput().midi);
        
        float gain = gainFunction();
        
        if (gain == lastGain)
        {
            if (gain == 0.0f)
                pc.buffers.audio.clear();
            else if (gain != 1.0f)
                pc.buffers.audio *= gain;
        }
        else
        {
            juce::SmoothedValue<float> smoother (lastGain);
            smoother.setTargetValue (gain);
            smoother.reset ((int) pc.buffers.audio.getNumSamples());
            pc.buffers.audio.multiplyBy (smoother);
        }
        
        lastGain = gain;
    }
    
private:
    Node* input = nullptr;
    std::function<float()> gainFunction;
    float lastGain = 0.0f;
};

//==============================================================================
//==============================================================================
class SendNode : public Node
{
public:
    SendNode (std::unique_ptr<Node> inputNode, int busIDToUse,
              std::function<float()> getGainFunc = nullptr)
        : input (std::move (inputNode)), busID (busIDToUse),
          gainFunction (std::move (getGainFunc))
    {
    }
    
    int getBusID() const
    {
        return busID;
    }
    
    std::function<float()> getGainFunction()
    {
        return gainFunction;
    }
                                                
    NodeProperties getNodeProperties() override
    {
        return input->getNodeProperties();
    }
    
    std::vector<Node*> getDirectInputNodes() override
    {
        return { input.get() };
    }

    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void process (const ProcessContext& pc) override
    {
        jassert (pc.buffers.audio.getNumChannels() == input->getProcessedOutput().audio.getNumChannels());

        // Just pass out input on to our output
        pc.buffers.audio.copyFrom (input->getProcessedOutput().audio);
        pc.buffers.midi.mergeFrom (input->getProcessedOutput().midi);
    }
    
private:
    std::unique_ptr<Node> input;
    const int busID;
    std::function<float()> gainFunction;
};


//==============================================================================
//==============================================================================
class ReturnNode : public Node
{
public:
    ReturnNode (std::unique_ptr<Node> inputNode, int busIDToUse)
        : input (std::move (inputNode)), busID (busIDToUse)
    {
    }
    
    NodeProperties getNodeProperties() override
    {
        NodeProperties props;
        props.hasAudio = false;
        props.hasMidi = false;
        props.numberOfChannels = 0;

        for (auto& node : getDirectInputNodes())
        {
            auto nodeProps = node->getNodeProperties();
            props.hasAudio = props.hasAudio | nodeProps.hasAudio;
            props.hasMidi = props.hasMidi | nodeProps.hasMidi;
            props.numberOfChannels = std::max (props.numberOfChannels, nodeProps.numberOfChannels);
            props.latencyNumSamples = std::max (props.latencyNumSamples, nodeProps.latencyNumSamples);
            hash_combine (props.nodeID, nodeProps.nodeID);
        }

        return props;
    }
    
    std::vector<Node*> getDirectInputNodes() override
    {
        return { input.get() };
    }
    
    bool transform (Node& rootNode) override
    {
        if (! hasInitialised)
        {
            findSendNodes (rootNode);
            return true;
        }
        
        return false;
    }

    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void process (const ProcessContext& pc) override
    {
        jassert (pc.buffers.audio.getNumChannels() == input->getProcessedOutput().audio.getNumChannels());

        // Copy the input on to our output, the SummingNode will copy all the sends and get all the input
        pc.buffers.audio.copyFrom (input->getProcessedOutput().audio);
        pc.buffers.midi.mergeFrom (input->getProcessedOutput().midi);
    }
    
private:
    std::unique_ptr<Node> input;
    const int busID;
    bool hasInitialised = false;
    
    void findSendNodes (Node& rootNode)
    {
        // This can only be initialised once as otherwise the latency nodes will get created again
        jassert (! hasInitialised);
        
        if (hasInitialised)
            return;
        
        std::vector<SendNode*> sends;
        visitNodes (rootNode,
                    [&] (Node& n)
                    {
                       if (auto send = dynamic_cast<SendNode*> (&n))
                           if (send->getBusID() == busID)
                               sends.push_back (send);
                    }, true);
        
        // Remove any send nodes that feed back in to this
        std::vector<Node*> sendsToRemove;
        
        for (auto send : sends)
        {
            visitNodes (*send,
                        [&] (Node& n)
                        {
                            if (&n == this)
                                sendsToRemove.push_back (send);
                        }, true);
        }
        
        sends.erase (std::remove_if (sends.begin(), sends.end(),
                                     [&] (auto n) { return std::find (sendsToRemove.begin(), sendsToRemove.end(), n) != sendsToRemove.end(); }),
                     sends.end());

        // Create a summing node if required
        if (sends.size() > 0)
        {
            std::vector<std::unique_ptr<Node>> ownedNodes;
            ownedNodes.push_back (std::move (input));
            
            // For each of the sends create a live gain node
            for (int i = (int) sends.size(); --i >= 0;)
            {
                auto gainFunction = sends[(size_t) i]->getGainFunction();
                
                if (! gainFunction)
                    continue;
                
                sends.erase (std::find (sends.begin(), sends.end(), sends[(size_t) i]));
                ownedNodes.push_back (makeNode<GainNode> (std::move (sends[(size_t) i]), std::move (gainFunction)));
            }
            
            auto node = makeNode<SummingNode> (std::move (ownedNodes));
            input.swap (node);
        }
        
        hasInitialised = true;
    }
};


//==============================================================================
//==============================================================================
/** Maps channels from one to another. */
class ChannelRemappingNode  : public Node
{
public:
    ChannelRemappingNode (std::unique_ptr<Node> inputNode,
                          std::vector<std::pair<int /*source channel*/, int /*dest channel*/>> channelMapToUse,
                          bool passMidiThrough)
        : input (std::move (inputNode)),
          channelMap (std::move (channelMapToUse)),
          passMIDI (passMidiThrough)
    {
        jassert (input);
    }
        
    NodeProperties getNodeProperties() override
    {
        NodeProperties props;
        props.hasAudio = ! channelMap.empty();
        props.hasMidi = passMIDI;
        props.numberOfChannels = 0;
        
        const auto inputProps = input->getNodeProperties();
        props.latencyNumSamples = inputProps.latencyNumSamples;
        
        props.nodeID = inputProps.nodeID;
        hash_combine (props.nodeID, passMIDI);
        
        // Num channels is the max of destinations
        for (auto channel : channelMap)
        {
            hash_combine (props.nodeID, channel.first);
            hash_combine (props.nodeID, channel.second);

            props.numberOfChannels = std::max (props.numberOfChannels, channel.second + 1);
        }
        
        return props;
    }
    
    std::vector<Node*> getDirectInputNodes() override
    {
        return { input.get() };
    }

    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void process (const ProcessContext& pc) override
    {
        auto inputBuffers = input->getProcessedOutput();
        
        // Pass on MIDI
        if (passMIDI)
            pc.buffers.midi.mergeFrom (inputBuffers.midi);
        
        // Remap audio
        auto sourceAudio = inputBuffers.audio;
        auto destAudio = pc.buffers.audio;

        for (auto channel : channelMap)
        {
            if ((int) sourceAudio.getNumChannels() <= channel.first)
                continue;
                
            auto sourceChanelBlock = sourceAudio.getSubsetChannelBlock ((size_t) channel.first, 1);
            auto destChanelBlock = destAudio.getSubsetChannelBlock ((size_t) channel.second, 1);
            destChanelBlock.add (sourceChanelBlock);
        }
    }

private:
    std::unique_ptr<Node> input;
    const std::vector<std::pair<int, int>> channelMap;
    const bool passMIDI;
};


/** Creates a channel map from source/dest pairs in an initializer_list. */
static inline std::vector<std::pair<int, int>> makeChannelMap (std::initializer_list<std::pair<int, int>> sourceDestChannelIndicies)
{
    std::vector<std::pair<int, int>> map;
    
    for (auto channelPair : sourceDestChannelIndicies)
        map.push_back (channelPair);
    
    return map;
}

//==============================================================================
//==============================================================================
/** Blocks audio and MIDI input from reaching the outputs. */
class SinkNode  : public Node
{
public:
    SinkNode (std::unique_ptr<Node> inputNode)
        : input (std::move (inputNode))
    {
        jassert (input);
    }
        
    NodeProperties getNodeProperties() override
    {
        constexpr size_t sinkNodeMagicHash = 0x95ab5e9dcd;
        auto props = input->getNodeProperties();
        hash_combine (props.nodeID, sinkNodeMagicHash);
        
        return props;
    }
    
    std::vector<Node*> getDirectInputNodes() override
    {
        return { input.get() };
    }

    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void process (const ProcessContext&) override
    {
    }

private:
    std::unique_ptr<Node> input;
};

}
