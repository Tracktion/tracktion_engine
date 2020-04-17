/*
  ==============================================================================

    TestAudioNodes.h
    Created: 15 Apr 2020 5:46:29pm
    Author:  David Rowland

  ==============================================================================
*/

#pragma once

#include "AudioNode.h"

//==============================================================================
//==============================================================================
/**
    Plays back a MIDI sequence.
    This simply plays it back from start to finish with no notation of a playhead.
*/
class MidiAudioNode : public AudioNode
{
public:
    MidiAudioNode (juce::MidiMessageSequence sequenceToPlay)
        : sequence (std::move (sequenceToPlay))
    {
    }
    
    AudioNodeProperties getAudioNodeProperties() override
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
        const int numSamples = (int) pc.buffers.audio.getNumSamples();
        const double blockDuration = numSamples / sampleRate;
        const auto timeRange = tracktion_engine::EditTimeRange::withStartAndLength (lastTime, blockDuration);
        
        for (int index = sequence.getNextIndexAtTime (timeRange.getStart()); index >= 0; ++index)
        {
            if (auto eventHolder = sequence.getEventPointer (index))
            {
                auto time = sequence.getEventTime (index);
                
                if (! timeRange.contains (time))
                    break;
                
                const int sampleNumber = sampleRate * (time - timeRange.getStart());
                jassert (sampleNumber < numSamples);
                pc.buffers.midi.addEvent (eventHolder->message, sampleNumber);
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
class SinAudioNode : public AudioNode
{
public:
    SinAudioNode (double frequency, int numChannelsToUse = 1)
        : numChannels (numChannelsToUse)
    {
        osc.setFrequency (frequency);
    }
    
    AudioNodeProperties getAudioNodeProperties() override
    {
        AudioNodeProperties props;
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
        osc.prepare ({ double (info.sampleRate), uint32 (info.blockSize), (uint32) numChannels });
    }
    
    void process (const ProcessContext& pc) override
    {
        auto block = pc.buffers.audio;
        osc.process (juce::dsp::ProcessContextReplacing<float> { block });
        jassert (pc.buffers.audio.getNumChannels() == getAudioNodeProperties().numberOfChannels);
    }
    
private:
    juce::dsp::Oscillator<float> osc { [] (float in) { return std::sin (in); } };
    const int numChannels;
};


//==============================================================================
//==============================================================================
class BasicSummingAudioNode : public AudioNode
{
public:
    BasicSummingAudioNode (std::vector<std::unique_ptr<AudioNode>> inputs)
        : nodes (std::move (inputs))
    {
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
        }

        return props;
    }
    
    std::vector<AudioNode*> getAllInputNodes() override
    {
        std::vector<AudioNode*> inputNodes;
        
        for (auto& node : nodes)
        {
            inputNodes.push_back (node.get());
            
            auto nodeInputs = node->getAllInputNodes();
            inputNodes.insert (inputNodes.end(), nodeInputs.begin(), nodeInputs.end());
        }

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
            
            pc.buffers.midi.addEvents (inputFromNode.midi, 0, -1, 0);
        }
    }

private:
    std::vector<std::unique_ptr<AudioNode>> nodes;
};


/** Creates a SummingAudioNode from a number of AudioNodes. */
std::unique_ptr<BasicSummingAudioNode> makeBaicSummingAudioNode (std::initializer_list<AudioNode*> nodes)
{
    std::vector<std::unique_ptr<AudioNode>> nodeVector;
    
    for (auto node : nodes)
        nodeVector.push_back (std::unique_ptr<AudioNode> (node));
        
    return std::make_unique<BasicSummingAudioNode> (std::move (nodeVector));
}


//==============================================================================
//==============================================================================
class FunctionAudioNode : public AudioNode
{
public:
    FunctionAudioNode (std::unique_ptr<AudioNode> input,
                       std::function<float (float)> fn)
        : node (std::move (input)),
          function (std::move (fn))
    {
        jassert (function);
    }
    
    AudioNodeProperties getAudioNodeProperties() override
    {
        return node->getAudioNodeProperties();
    }
    
    std::vector<AudioNode*> getAllInputNodes() override
    {
        std::vector<AudioNode*> inputNodes;
        inputNodes.push_back (node.get());
        
        auto nodeInputs = node->getAllInputNodes();
        inputNodes.insert (inputNodes.end(), nodeInputs.begin(), nodeInputs.end());

        return inputNodes;
    }

    bool isReadyToProcess() override
    {
        return node->hasProcessed();
    }
    
    void process (const ProcessContext& pc) override
    {
        auto inputBuffer = node->getProcessedOutput().audio;
        jassert (inputBuffer.getNumSamples() == pc.buffers.audio.getNumSamples());

        const int numSamples = (int) pc.buffers.audio.getNumSamples();
        const int numChannels = std::min ((int) inputBuffer.getNumChannels(), (int) pc.buffers.audio.getNumChannels());

        for (int c = 0; c < numChannels; ++c)
        {
            const float* inputSamples = inputBuffer.getChannelPointer ((size_t) c);
            float* outputSamples = pc.buffers.audio.getChannelPointer ((size_t) c);
            
            for (int i = 0; i < numSamples; ++i)
                outputSamples[i] = function (inputSamples[i]);
        }
    }
    
private:
    std::unique_ptr<AudioNode> node;
    std::function<float (float)> function;
};

/** Returns an audio node that applied a fixed gain to an input node. */
std::unique_ptr<AudioNode> makeGainAudioNode (std::unique_ptr<AudioNode> input, float gain)
{
    return makeAudioNode<FunctionAudioNode> (std::move (input), [gain] (float s) { return s * gain; });
}


//==============================================================================
//==============================================================================
class SendAudioNode : public AudioNode
{
public:
    SendAudioNode (std::unique_ptr<AudioNode> inputNode, int busIDToUse)
        : input (std::move (inputNode)), busID (busIDToUse)
    {
    }
    
    int getBusID() const
    {
        return busID;
    }
    
    AudioNodeProperties getAudioNodeProperties() override
    {
        return input->getAudioNodeProperties();
    }
    
    std::vector<AudioNode*> getAllInputNodes() override
    {
        std::vector<AudioNode*> inputNodes;
        inputNodes.push_back (input.get());
        
        auto nodeInputs = input->getAllInputNodes();
        inputNodes.insert (inputNodes.end(), nodeInputs.begin(), nodeInputs.end());

        return inputNodes;
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
        pc.buffers.midi.addEvents (input->getProcessedOutput().midi, 0, (int) pc.buffers.audio.getNumSamples(), 0);
    }
    
private:
    std::unique_ptr<AudioNode> input;
    const int busID;
};


//==============================================================================
//==============================================================================
class ReturnAudioNode : public AudioNode
{
public:
    ReturnAudioNode (std::unique_ptr<AudioNode> inputNode, int busIDToUse)
        : input (std::move (inputNode)), busID (busIDToUse)
    {
    }
    
    AudioNodeProperties getAudioNodeProperties() override
    {
        return input->getAudioNodeProperties();
    }
    
    std::vector<AudioNode*> getAllInputNodes() override
    {
        std::vector<AudioNode*> inputNodes;
        inputNodes.push_back (input.get());
        
        auto nodeInputs = input->getAllInputNodes();
        inputNodes.insert (inputNodes.end(), nodeInputs.begin(), nodeInputs.end());

        return inputNodes;
    }

    bool isReadyToProcess() override
    {
        for (auto send : sendNodes)
            if (! send->hasProcessed())
                return false;
        
        return input->hasProcessed();
    }
    
    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        for (auto node : info.allNodes)
            if (auto send = dynamic_cast<SendAudioNode*> (node))
                if (send->getBusID() == busID)
                    sendNodes.push_back (send);
    }
    
    void process (const ProcessContext& pc) override
    {
        jassert (pc.buffers.audio.getNumChannels() == input->getProcessedOutput().audio.getNumChannels());
        
        // Copy the input on to our output
        pc.buffers.audio.copyFrom (input->getProcessedOutput().audio);
        pc.buffers.midi.addEvents (input->getProcessedOutput().midi, 0, (int) pc.buffers.audio.getNumSamples(), 0);
        
        // And a copy of all the send nodes
        for (auto send : sendNodes)
        {
            const auto numChannelsToUse = std::min (pc.buffers.audio.getNumChannels(), send->getProcessedOutput().audio.getNumChannels());
            
            if (numChannelsToUse > 0)
                pc.buffers.audio.getSubsetChannelBlock (0, numChannelsToUse)
                    .add (send->getProcessedOutput().audio.getSubsetChannelBlock (0, numChannelsToUse));
            
            pc.buffers.midi.addEvents (send->getProcessedOutput().midi, 0, (int) pc.buffers.audio.getNumSamples(), 0);
        }
    }
    
private:
    std::unique_ptr<AudioNode> input;
    std::vector<AudioNode*> sendNodes;
    const int busID;
};


/** Returns a node wrapped in another node that applies a static gain to it. */
std::unique_ptr<AudioNode> makeGainNode (std::unique_ptr<AudioNode> node, float gain)
{
    return std::make_unique<FunctionAudioNode> (std::move (node), [gain] (float s) { return s * gain; });
}


//==============================================================================
//==============================================================================
/** Maps channels from one to another. */
class ChannelMappingAudioNode : public AudioNode
{
public:
    ChannelMappingAudioNode (std::unique_ptr<AudioNode> inputNode,
                             std::vector<std::pair<int /* source channel */, int /* dest channel */>> channelMapToUse,
                             bool passMidiThrough)
        : input (std::move (inputNode)),
          channelMap (std::move (channelMapToUse)),
          passMIDI (passMidiThrough)
    {
    }
        
    AudioNodeProperties getAudioNodeProperties() override
    {
        AudioNodeProperties props;
        props.hasAudio = true;
        props.hasMidi = false;
        props.numberOfChannels = 0;
        props.latencyNumSamples = input->getAudioNodeProperties().latencyNumSamples;

        // Num channels is the max of destinations
        for (auto channel : channelMap)
            props.numberOfChannels = std::max (props.numberOfChannels, channel.second + 1);
        
        return props;
    }
    
    std::vector<AudioNode*> getAllInputNodes() override
    {
        std::vector<AudioNode*> inputNodes;
        inputNodes.push_back (input.get());
        
        auto nodeInputs = input->getAllInputNodes();
        inputNodes.insert (inputNodes.end(), nodeInputs.begin(), nodeInputs.end());

        return inputNodes;
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
            pc.buffers.midi.addEvents (inputBuffers.midi, 0, -1, 0);
        
        // Remap audio
        auto sourceAudio = inputBuffers.audio;
        auto destAudio = pc.buffers.audio;

        for (auto channel : channelMap)
        {
            auto sourceChanelBlock = sourceAudio.getSubsetChannelBlock (channel.first, 1);
            auto destChanelBlock = destAudio.getSubsetChannelBlock (channel.second, 1);
            destChanelBlock.add (sourceChanelBlock);
        }
    }

private:
    std::unique_ptr<AudioNode> input;
    const std::vector<std::pair<int, int>> channelMap;
    const bool passMIDI;
};


/** Creates a channel map from source/dest pairs in an initializer_list. */
std::vector<std::pair<int, int>> makeChannelMap (std::initializer_list<std::pair<int, int>> sourceDestChannelIndicies)
{
    std::vector<std::pair<int, int>> map;
    
    for (auto channelPair : sourceDestChannelIndicies)
        map.push_back (channelPair);
    
    return map;
}
