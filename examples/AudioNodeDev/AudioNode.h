/*
  ==============================================================================

    AudioNode.h
    Created: 15 Apr 2020 10:42:56am
    Author:  David Rowland

  ==============================================================================
*/

#pragma once

/**
    Aims:
    - Separate graph structure from processing and any model
    - Ensure nodes can be processed multi-threaded which scales independantly of graph complexity
    - Processing can happen in any sized block (up to the maximum prepared for)
    - Process calls will only ever get the number of channels to fill that they report
    - Process calls will always provide empty buffers so nodes can simply "add" in to them. (Measure performance here)
    - Processing in float or double

    Notes:
    - Each node should have pointers to its inputs
    - When a node is processed, it should check its inputs to see if they have produced outputs
    - If they have, that node can be processed. If they haven't the processor can try another node
    - If one node reports latency, every other node being summed with it will need to be delayed up to the same ammount
    - The reported latency of a node is the max of all its input latencies
 
    Initialisation:
    - There really needs to be two stages for this:
        - Stage 1 is a simple visit of all the base nodes (i.e. no send/return connections).
          This will get a list of all the nodes in the graph
        - Stage 2 will be the initialise call with a flat list of all the nodes.
          Send/return nodes can use this to add inputs from the graph (e.g. based on busNum)
          (However, at this stage, return nodes might choose to add latency nodes to balance themselves???)
          (Maybe this should have to happen when the graph is built? That would mean latency needs to be fixed.)
    - Now we have a fully connected graph we can do a proper DFS:
        - This can be used to balance processing buffers between the graph by finding unconnected nodes
        - Finding a processing order by building "stacks" of nodes starting with leafs
    - This might look like the following steps:
        - visitAcyclic (Visitor) [only visit nodes that it was constructed with, used to create the flat list]
        - initialise (const PlaybackInitialisationInfo&) [used to add extra connections and balance latency etc.]
        - visitDFS (Visitor) [used to get list of nodes to process and can be called multiple times to optomise graph etc.]
 
    Each node needs:
    - A flag to say if it has produced outputs yet
    - A method to report its latency
    - A method to process it
*/

#include <JuceHeader.h>

class AudioNode;

//==============================================================================
/** Passed into AudioNodes when they are being initialised, to give them useful
    contextual information that they may need
*/
struct PlaybackInitialisationInfo
{
    double sampleRate;
    int blockSize;
    const std::vector<AudioNode*>& allNodes;
};

/** Holds some really basic properties of a node */
struct AudioNodeProperties
{
    bool hasAudio;
    bool hasMidi;
    int numberOfChannels;
    int latencyNumSamples = 0;
};

//==============================================================================
//==============================================================================
class AudioNode
{
public:
    AudioNode() = default;
    virtual ~AudioNode() = default;
    
    //==============================================================================
    /** Call once after the graph has been constructed to initialise buffers etc. */
    void initialise (const PlaybackInitialisationInfo&);
    
    /** Call before processing the next block, used to reset the process status. */
    void prepareForNextBlock();
    
    /** Call to process the node, which will in turn call the process method with the buffers to fill. */
    void process (int numSamples);
    
    /** Returns true if this node has processed and its outputs can be retrieved. */
    bool hasProcessed() const;
    
    /** Contains the buffers for a processing operation. */
    struct AudioAndMidiBuffer
    {
        juce::dsp::AudioBlock<float> audio;
        MidiBuffer& midi;
    };

    /** Returns the processed audio and MIDI output.
        Must only be called after hasProcessed returns true.
    */
    AudioAndMidiBuffer getProcessedOutput();

    //==============================================================================
    /** Should return the properties of the node. */
    virtual AudioNodeProperties getAudioNodeProperties() = 0;

    /** Should return all the inputs feeding in to this node. */
    virtual std::vector<AudioNode*> getAllInputNodes() { return {}; }

    /** Called once before playback begins for each node.
        Use this to allocate buffers etc.
    */
    virtual void prepareToPlay (const PlaybackInitialisationInfo&) {}

    /** Should return true when this node is ready to be processed.
        This is usually when its input's output buffers are ready.
    */
    virtual bool isReadyToProcess() = 0;
    
    /** Struct to describe a single iteration of a process call. */
    struct ProcessContext
    {
        AudioAndMidiBuffer buffers;
    };
    
    /** Called when the node is to be processed.
        This should add in to the buffers available making sure not to change their size at all.
    */
    virtual void process (const ProcessContext&) = 0;

private:
    std::atomic<bool> hasBeenProcessed { false };
    AudioBuffer<float> audioBuffer;
    MidiBuffer midiBuffer;
    int numSamplesProcessed = 0;
};

void AudioNode::initialise (const PlaybackInitialisationInfo& info)
{
    auto props = getAudioNodeProperties();
    audioBuffer.setSize (props.numberOfChannels, info.blockSize);
}

void AudioNode::prepareForNextBlock()
{
    hasBeenProcessed = false;
}

void AudioNode::process (int numSamples)
{
    audioBuffer.clear();
    midiBuffer.clear();
    const int numChannelsBeforeProcessing = audioBuffer.getNumChannels();
    const int numSamplesBeforeProcessing = audioBuffer.getNumSamples();
    ignoreUnused (numChannelsBeforeProcessing, numSamplesBeforeProcessing);

    ProcessContext pc { { juce::dsp::AudioBlock<float> (audioBuffer).getSubBlock (0, numSamples) , midiBuffer } };
    process (pc);
    numSamplesProcessed = numSamples;
    hasBeenProcessed = true;
    
    jassert (numChannelsBeforeProcessing == audioBuffer.getNumChannels());
    jassert (numSamplesBeforeProcessing == audioBuffer.getNumSamples());
}

bool AudioNode::hasProcessed() const
{
    return hasBeenProcessed;
}

AudioNode::AudioAndMidiBuffer AudioNode::getProcessedOutput()
{
    jassert (hasProcessed());
    return { juce::dsp::AudioBlock<float> (audioBuffer).getSubBlock (0, numSamplesProcessed), midiBuffer };
}
