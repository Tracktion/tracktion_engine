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
/**
    Plays back a MIDI sequence.
    This simply plays it back from start to finish with no notation of a playhead.
*/
class MidiNode final    : public Node
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
    
    void process (ProcessContext& pc) override
    {
        const int numSamples = (int) pc.referenceSampleRange.getLength();
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
class SinNode final : public Node
{
public:
    SinNode (float frequencyToUse, int numChannelsToUse = 1, size_t nodeIDToUse = 0)
        : numChannels (numChannelsToUse), nodeID (nodeIDToUse), frequency (frequencyToUse)
    {
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
        oscillator.reset (frequency, info.sampleRate);
    }

    void process (ProcessContext& pc) override
    {
        setAllFrames (pc.buffers.audio, [&] { return oscillator.getNext(); });
    }
    
private:
    const int numChannels;
    size_t nodeID = 0;
    test_utilities::SineOscillator oscillator;
    float frequency = 0;
};


//==============================================================================
//==============================================================================
/**
    Just a simple audio node that doesn't take any input so can be used as a stub.
*/
class SilentNode final  : public Node
{
public:
    SilentNode (int numChannelsToUse = 1)
        : numChannels (numChannelsToUse)
    {
        setOptimisations ({ ClearBuffers::no,
                            AllocateAudioBuffer::no });
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
    
    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        audioBuffer.resize (choc::buffer::Size::create ((choc::buffer::ChannelCount) numChannels,
                                                        (choc::buffer::FrameCount) info.blockSize));
        audioBuffer.clear();
    }
    
    void process (ProcessContext& pc) override
    {
        pc.buffers.midi.clear();
        setAudioOutput (nullptr, audioBuffer.getView().getStart (pc.buffers.audio.getNumFrames()));
    }
    
private:
    const int numChannels;
    choc::buffer::ChannelArrayBuffer<float> audioBuffer;
};


//==============================================================================
//==============================================================================
class BasicSummingNode final    : public Node
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
            props.hasAudio = props.hasAudio || nodeProps.hasAudio;
            props.hasMidi = props.hasMidi || nodeProps.hasMidi;
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

    void process (ProcessContext& pc) override
    {
        const auto numChannels = pc.buffers.audio.getNumChannels();

        // Get each of the inputs and add them to dest
        for (auto& node : nodes)
        {
            auto inputFromNode = node->getProcessedOutput();
            
            if (auto numChannelsToAdd = std::min (inputFromNode.audio.getNumChannels(), numChannels))
                add (pc.buffers.audio.getFirstChannels (numChannelsToAdd),
                     inputFromNode.audio.getFirstChannels (numChannelsToAdd));
            
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
class FunctionNode final    : public Node
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
    
    void process (ProcessContext& pc) override
    {
        auto inputBuffer = node->getProcessedOutput().audio;

        auto numFrames = pc.referenceSampleRange.getLength();
        const auto numChannels = std::min (inputBuffer.getNumChannels(), pc.buffers.audio.getNumChannels());
        jassert (inputBuffer.getNumFrames() == numFrames);

        for (choc::buffer::ChannelCount c = 0; c < numChannels; ++c)
        {
            auto destIter = pc.buffers.audio.getIterator (c);
            auto sourceIter = inputBuffer.getIterator (c);

            for (choc::buffer::FrameCount i = 0; i < numFrames; ++i)
                *destIter++ = function (*sourceIter++);
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
class GainNode final : public Node
{
public:
    /** Creates a GainNode that doesn't own its input. */
    GainNode (Node* inputNode, std::function<float()> gainFunc)
        : input (inputNode), gainFunction (std::move (gainFunc))
    {
        assert (input != nullptr);
        assert (gainFunction);
        lastGain = gainFunction();
    }

    /** Creates a GainNode that owns its input. */
    GainNode (std::unique_ptr<Node> inputNode, std::function<float()> gainFunc)
        : ownedInput (std::move (inputNode)), input (ownedInput.get()),
          gainFunction (std::move (gainFunc))
    {
        assert (ownedInput != nullptr);
        assert (input != nullptr);
        assert (gainFunction);
        lastGain = gainFunction();
    }

    NodeProperties getNodeProperties() override
    {
        auto props = input->getNodeProperties();
        constexpr size_t gainNodeMagicHash = 0x6761696e4e6f6465;
        
        if (props.nodeID != 0)
            hash_combine (props.nodeID, gainNodeMagicHash);
        
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
    
    void process (ProcessContext& pc) override
    {
        jassert (pc.buffers.audio.getNumChannels() == input->getProcessedOutput().audio.getNumChannels());

        // Just pass out input on to our output
        copy (pc.buffers.audio, input->getProcessedOutput().audio);
        pc.buffers.midi.mergeFrom (input->getProcessedOutput().midi);
        
        float gain = gainFunction();
        
        if (gain == lastGain)
        {
            if (gain == 0.0f)
                pc.buffers.audio.clear();
            else if (gain != 1.0f)
                applyGain (pc.buffers.audio, gain);
        }
        else
        {
            juce::SmoothedValue<float> smoother (lastGain);
            smoother.setTargetValue (gain);
            smoother.reset ((int) pc.buffers.audio.getNumFrames());
            applyGainPerFrame (pc.buffers.audio, [&] { return smoother.getNextValue(); });
        }
        
        lastGain = gain;
    }
    
private:
    std::unique_ptr<Node> ownedInput;
    Node* input = nullptr;
    std::function<float()> gainFunction;
    float lastGain = 0.0f;
};

//==============================================================================
//==============================================================================
class SendNode  : public Node
{
public:
    SendNode (std::unique_ptr<Node> inputNode, int busIDToUse,
              std::function<float()> getGainFunc = nullptr)
        : input (std::move (inputNode)), busID (busIDToUse),
          gainFunction (std::move (getGainFunc))
    {
        setOptimisations ({ ClearBuffers::no,
                            AllocateAudioBuffer::no });
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
        auto props = input->getNodeProperties();
        constexpr size_t sendNodeMagicHash = 0x73656e644e6f6465;
        
        if (props.nodeID != 0)
            hash_combine (props.nodeID, sendNodeMagicHash);
        
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
    
    void process (ProcessContext& pc) override
    {
        auto source = input->getProcessedOutput();
        jassert (pc.buffers.audio.getSize() == source.audio.getSize());

        // Just pass out input on to our output
        // N.B We need to clear manually here due to optimisations
        setAudioOutput (input.get(), source.audio);

        // If the source only outputs to this node, we can steal its data
        if (numOutputNodes == 1)
            pc.buffers.midi.swapWith (source.midi);
        else
            pc.buffers.midi.copyFrom (source.midi);
    }
    
private:
    std::unique_ptr<Node> input;
    const int busID;
    std::function<float()> gainFunction;
};


//==============================================================================
//==============================================================================
class ReturnNode final  : public Node
{
public:
    ReturnNode (int busIDToUse)
        : busID (busIDToUse)
    {
        setOptimisations ({ ClearBuffers::no,
                            AllocateAudioBuffer::yes });
    }

    ReturnNode (std::unique_ptr<Node> inputNode, int busIDToUse)
        : input (std::move (inputNode)), busID (busIDToUse)
    {
        setOptimisations ({ ClearBuffers::no,
                            AllocateAudioBuffer::yes });
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
            props.hasAudio = props.hasAudio || nodeProps.hasAudio;
            props.hasMidi = props.hasMidi || nodeProps.hasMidi;
            props.numberOfChannels = std::max (props.numberOfChannels, nodeProps.numberOfChannels);
            props.latencyNumSamples = std::max (props.latencyNumSamples, nodeProps.latencyNumSamples);
            hash_combine (props.nodeID, nodeProps.nodeID);
        }
        
        constexpr size_t returnNodeMagicHash = 0x72657475726e;
        
        if (props.nodeID != 0)
            hash_combine (props.nodeID, returnNodeMagicHash);

        return props;
    }
    
    std::vector<Node*> getDirectInputNodes() override
    {
        if (! input)
            return {};

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
        return ! input || input->hasProcessed();
    }
    
    void process (ProcessContext& pc) override
    {
        if (! input)
        {
            // N.B We need to clear manually here due to optimisations
            pc.buffers.midi.clear();
            pc.buffers.audio.clear();

            return;
        }

        auto source = input->getProcessedOutput();
        jassert (pc.buffers.audio.getSize() == source.audio.getSize());

        // Copy the input on to our output, the SummingNode will copy all the sends and get all the input
        setAudioOutput (input.get(), source.audio);
        pc.buffers.midi.copyFrom (source.midi);
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

            if (input)
                ownedNodes.push_back (std::move (input));
            
            // For each of the sends create a live gain node
            for (int i = (int) sends.size(); --i >= 0;)
            {
                auto gainFunction = sends[(size_t) i]->getGainFunction();
                
                if (! gainFunction)
                    continue;
                
                ownedNodes.push_back (makeNode<GainNode> (std::move (sends[(size_t) i]), std::move (gainFunction)));
				sends.erase (std::find (sends.begin(), sends.end(), sends[(size_t) i]));
			}

            auto node = makeNode<SummingNode> (std::move (ownedNodes), std::vector<Node*> (sends.begin(), sends.end()));
            input.swap (node);
        }
        
        hasInitialised = true;
    }
};


//==============================================================================
//==============================================================================
/** Maps channels from one to another. */
class ChannelRemappingNode final    : public Node
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
        
        constexpr size_t channelNodeMagicHash = 0x6368616e6e656c;
        
        if (props.nodeID != 0)
            hash_combine (props.nodeID, channelNodeMagicHash);

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
    
    void process (ProcessContext& pc) override
    {
        auto inputBuffers = input->getProcessedOutput();
        
        // Pass on MIDI
        if (passMIDI)
            pc.buffers.midi.mergeFrom (inputBuffers.midi);
        
        // Remap audio
        for (auto channel : channelMap)
            if (channel.first < (int) inputBuffers.audio.getNumChannels())
                add (pc.buffers.audio.getChannel ((choc::buffer::ChannelCount) channel.second),
                     inputBuffers.audio.getChannel ((choc::buffer::ChannelCount) channel.first));
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
class SinkNode final    : public Node
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
    
    void process (ProcessContext&) override
    {
    }

private:
    std::unique_ptr<Node> input;
};


//==============================================================================
//==============================================================================
/** Takes a non-owning input node and simply forwards its outputs on. */
class ForwardingNode final  : public tracktion::graph::Node
{
public:
    ForwardingNode (tracktion::graph::Node* inputNode)
        : input (inputNode)
    {
        jassert (input);
    }

    ForwardingNode (std::shared_ptr<tracktion::graph::Node> inputNode)
        : ForwardingNode (inputNode.get())
    {
        nodePtr = std::move (inputNode);
    }

    tracktion::graph::NodeProperties getNodeProperties() override
    {
        auto props = input->getNodeProperties();
        hash_combine (props.nodeID, nodeID);
        
        return props;
    }
    
    std::vector<tracktion::graph::Node*> getDirectInputNodes() override
    {
        return { input };
    }

    bool isReadyToProcess() override
    {
        return input->hasProcessed();
    }
    
    void process (ProcessContext& pc) override
    {
        auto inputBuffers = input->getProcessedOutput();
        copy (pc.buffers.audio, inputBuffers.audio);
        pc.buffers.midi.copyFrom (inputBuffers.midi);
    }

private:
    tracktion::graph::Node* input;
    std::shared_ptr<tracktion::graph::Node> nodePtr;
    const size_t nodeID { (size_t) juce::Random::getSystemRandom().nextInt() };
};

}}
