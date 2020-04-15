/*
  ==============================================================================

    RackNode.cpp
    Created: 8 Apr 2020 3:41:40pm
    Author:  David Rowland

  ==============================================================================
*/

#include "RackNode.h"

/**
    Aims:
    - Separate graph structure from processing and any model
    - Ensure nodes can be processed multi-threaded which scales independantly of graph complexity
    - Processing can happen in any sized block
    - Processing in float or double
 
    Notes:
    - Each node should have pointers to its inputs
    - When a node is processed, it should check its inputs to see if they have produced outputs
    - If they have, that node can be processed. If they haven't the processor can try another node
    - If one node reports latency, every other node being summed with it will need to be delayed up to the same ammount
    - The reported latency of a node is the max of all its input latencies
 
    Each node needs:
    - A flag to say if it has produced outputs yet
    - A method to report its latency
    - A method to process it
*/

namespace utilities
{
    void copyAudioBuffer (AudioBuffer<float>& dest, const AudioBuffer<float>& source)
    {
        jassert (source.getNumSamples() == dest.getNumSamples());
        const int numSamples = dest.getNumSamples();
        const int numChannels = std::min (dest.getNumChannels(), source.getNumChannels());

        for (int i = 0; i < numChannels; ++i)
            dest.copyFrom (i, 0, source, i, 0, numSamples);
    }

    void copyAudioBuffer (AudioBuffer<float>& dest, const juce::dsp::AudioBlock<float>& source)
    {
        jassert (source.getNumSamples() == dest.getNumSamples());
        const int numChannels = std::min (dest.getNumChannels(), (int) source.getNumChannels());
        
        juce::dsp::AudioBlock<float> (dest).copyFrom (source.getSubsetChannelBlock (0, numChannels));
    }

    void addAudioBuffer (AudioBuffer<float>& dest, const AudioBuffer<float>& source)
    {
        jassert (source.getNumSamples() == dest.getNumSamples());
        const int numSamples = dest.getNumSamples();
        const int numChannels = std::min (dest.getNumChannels(), source.getNumChannels());

        for (int i = 0; i < numChannels; ++i)
            dest.addFrom (i, 0, source, i, 0, numSamples);
    }

    void addAudioBuffer (AudioBuffer<float>& dest, const juce::dsp::AudioBlock<float>& source)
    {
        jassert (source.getNumSamples() == dest.getNumSamples());
        const int numChannels = std::min (dest.getNumChannels(), (int) source.getNumChannels());
        
        juce::dsp::AudioBlock<float> (dest).add (source.getSubsetChannelBlock (0, numChannels));
    }

    void writeToFile (File file, const AudioBuffer<float>& buffer, double sampleRate)
    {
        if (auto writer = std::unique_ptr<AudioFormatWriter> (WavAudioFormat().createWriterFor (file.createOutputStream().release(),
                                                                                                sampleRate, buffer.getNumChannels(), 16, {}, 0)))
        {
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        }
    }
}

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
    struct ProcessContext
    {
        juce::dsp::AudioBlock<float> audio;
        MidiBuffer& midi;
    };

    /** Returns the processed audio and MIDI output.
        Must only be called after hasProcessed returns true.
    */
    ProcessContext getProcessedOutput();

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
    
    /** Called when the node is to be processed.
        This should add in to the buffers available making sure not to change their size at all.
    */
    virtual void process (AudioBuffer<float>& destAudio, MidiBuffer& destMidi) = 0;

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

    AudioBuffer<float> subSectionBuffer (audioBuffer.getArrayOfWritePointers(),
                                         audioBuffer.getNumChannels(), 0, numSamples);
    process (subSectionBuffer, midiBuffer);
    numSamplesProcessed = numSamples;
    hasBeenProcessed = true;
    
    jassert (numChannelsBeforeProcessing == audioBuffer.getNumChannels());
    jassert (numSamplesBeforeProcessing == audioBuffer.getNumSamples());
}

bool AudioNode::hasProcessed() const
{
    return hasBeenProcessed;
}

AudioNode::ProcessContext AudioNode::getProcessedOutput()
{
    jassert (hasProcessed());
    return { juce::dsp::AudioBlock<float> (audioBuffer).getSubBlock (0, numSamplesProcessed), midiBuffer };
}


//==============================================================================
//==============================================================================
/**
    Simple processor for an AudioNode.
    This simply iterate all the nodes attempting to process them in a single thread.
*/
class AudioNodeProcessor
{
public:
    /** Creates an AudioNodeProcessor to process an AudioNode. */
    AudioNodeProcessor (std::unique_ptr<AudioNode> nodeToProcess)
        : node (std::move (nodeToProcess))
    {
        auto nodes = node->getAllInputNodes();
        nodes.push_back (node.get());
        std::unique_copy (nodes.begin(), nodes.end(), std::back_inserter (allNodes),
                          [] (auto n1, auto n2) { return n1 == n2; });
    }

    /** Preapres the processor to be played. */
    void prepareToPlay (double sampleRate, int blockSize)
    {
        const PlaybackInitialisationInfo info { sampleRate, blockSize, allNodes };
        
        for (auto& node : allNodes)
        {
            node->initialise (info);
            node->prepareToPlay (info);
        }
    }

    /** Processes an block of audio and MIDI data. */
    void process (AudioBuffer<float>& audio, MidiBuffer& midi)
    {
        for (auto node : allNodes)
            node->prepareForNextBlock();
        
        for (;;)
        {
            int processedAnyNodes = false;
            
            for (auto node : allNodes)
            {
                if (! node->hasProcessed() && node->isReadyToProcess())
                {
                    node->process (audio.getNumSamples());
                    processedAnyNodes = true;
                }
            }
            
            if (! processedAnyNodes)
            {
                utilities::copyAudioBuffer (audio, node->getProcessedOutput().audio);
                //TODO: MIDI
                
                break;
            }
        }
    }
    
private:
    std::unique_ptr<AudioNode> node;
    std::vector<AudioNode*> allNodes;
};


//==============================================================================
//==============================================================================
class LatencyAudioNode  : public AudioNode
{
public:
    LatencyAudioNode (std::unique_ptr<AudioNode> inputNode, int numSamplesToDelay)
        : input (std::move (inputNode)), latencyNumSamples (numSamplesToDelay)
    {
    }
    
    AudioNodeProperties getAudioNodeProperties() override
    {
        auto props = input->getAudioNodeProperties();
        props.latencyNumSamples = std::max (props.latencyNumSamples, latencyNumSamples);
        
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
    
    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        fifo.setSize (getAudioNodeProperties().numberOfChannels, latencyNumSamples + info.blockSize + 1);
        fifo.writeSilence (latencyNumSamples);
        jassert (fifo.getNumReady() == latencyNumSamples);
    }
    
    void process (AudioBuffer<float>& outputBuffer, MidiBuffer&) override
    {
        auto inputBuffer = input->getProcessedOutput().audio;
        jassert (fifo.getNumChannels() == inputBuffer.getNumChannels());
        fifo.write (inputBuffer);
        
        jassert (fifo.getNumReady() >= outputBuffer.getNumSamples());
        jassert (inputBuffer.getNumSamples() == outputBuffer.getNumSamples());

        fifo.readAdding (outputBuffer, 0);
        
        //TODO: MIDI
    }
    
private:
    std::unique_ptr<AudioNode> input;
    const int latencyNumSamples;
    tracktion_engine::AudioFifo fifo { 1, 32 };
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
        : nodes (std::move (inputs))
    {
        const int maxLatency = getAudioNodeProperties().latencyNumSamples;
        
        for (auto& node : nodes)
        {
            const int nodeLatency = node->getAudioNodeProperties().latencyNumSamples;
            const int latencyToAdd = maxLatency - nodeLatency;
            
            if (latencyToAdd == 0)
                continue;
            
            std::unique_ptr<AudioNode> latencyNode = std::make_unique<LatencyAudioNode> (std::move (node), latencyToAdd);
            node.swap (latencyNode);
        }
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
    
    void process (AudioBuffer<float>& dest, MidiBuffer& midi) override
    {
        // Get each of the inputs and add them to dest
        for (auto& node : nodes)
        {
            utilities::addAudioBuffer (dest, node->getProcessedOutput().audio);
            //TODO:  MIDI
        }
    }

private:
    std::vector<std::unique_ptr<AudioNode>> nodes;
};


/** Creates a SummingAudioNode from a number of AudioNodes. */
std::unique_ptr<SummingAudioNode> makeSummingAudioNode (std::initializer_list<AudioNode*> nodes)
{
    std::vector<std::unique_ptr<AudioNode>> nodeVector;
    
    for (auto node : nodes)
        nodeVector.push_back (std::unique_ptr<AudioNode> (node));
        
    return std::make_unique<SummingAudioNode> (std::move (nodeVector));
}


//==============================================================================
//==============================================================================
class SinAudioNode : public AudioNode
{
public:
    SinAudioNode (double frequency)
    {
        osc.setFrequency (frequency);
    }
    
    AudioNodeProperties getAudioNodeProperties() override
    {
        AudioNodeProperties props;
        props.hasAudio = true;
        props.hasMidi = false;
        props.numberOfChannels = 1;
        
        return props;
    }
    
    bool isReadyToProcess() override
    {
        return true;
    }
    
    void prepareToPlay (const PlaybackInitialisationInfo& info) override
    {
        osc.prepare ({ double (info.sampleRate), uint32 (info.blockSize), 1 });
    }
    
    void process (AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        float* samples = buffer.getWritePointer (0);
        int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
            samples[i] = osc.processSample (0.0);
    }
    
private:
    juce::dsp::Oscillator<float> osc { [] (float in) { return std::sin (in); } };
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
    
    void process (AudioBuffer<float>& dest, MidiBuffer& midi) override
    {
        // Get each of the inputs and add them to dest
        for (auto& node : nodes)
        {
            utilities::addAudioBuffer (dest, node->getProcessedOutput().audio);
            //TODO:  MIDI
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
    
    void process (AudioBuffer<float>& outputBuffer, MidiBuffer&) override
    {
        auto inputBuffer = node->getProcessedOutput().audio;
        jassert (inputBuffer.getNumSamples() == outputBuffer.getNumSamples());

        const int numSamples = outputBuffer.getNumSamples();
        const int numChannels = std::min ((int) inputBuffer.getNumChannels(), outputBuffer.getNumChannels());

        for (int c = 0; c < numChannels; ++c)
        {
            const float* inputSamples = inputBuffer.getChannelPointer ((size_t) c);
            float* outputSamples = outputBuffer.getWritePointer (c);
            
            for (int i = 0; i < numSamples; ++i)
                outputSamples[i] = function (inputSamples[i]);
        }
    }
    
private:
    std::unique_ptr<AudioNode> node;
    std::function<float (float)> function;
};


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
    
    void process (AudioBuffer<float>& outputBuffer, MidiBuffer& outputMidi) override
    {
        // Just pass out input on to our output
        utilities::copyAudioBuffer (outputBuffer, input->getProcessedOutput().audio);
        outputMidi.addEvents (input->getProcessedOutput().midi, 0, outputBuffer.getNumSamples(), 0);
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
    
    void process (AudioBuffer<float>& outputBuffer, MidiBuffer& outputMidi) override
    {
        // Copy the input on to our output
        utilities::copyAudioBuffer (outputBuffer, input->getProcessedOutput().audio);
        outputMidi.addEvents (input->getProcessedOutput().midi, 0, outputBuffer.getNumSamples(), 0);
        
        // And a copy of all the send nodes
        for (auto send : sendNodes)
        {
            utilities::addAudioBuffer (outputBuffer, send->getProcessedOutput().audio);
            outputMidi.addEvents (send->getProcessedOutput().midi, 0, outputBuffer.getNumSamples(), 0);
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
class AudioNodeTests : public juce::UnitTest
{
public:
    AudioNodeTests()
        : juce::UnitTest ("AudioNode", "AudioNode")
    {
    }
    
    void runTest() override
    {
        for (bool randomiseBlockSizes : { false, true })
        {
            runSinTest (randomiseBlockSizes);
            runSinCancellingTest (randomiseBlockSizes);
            runSinOctaveTest (randomiseBlockSizes);
            runSendReturnTest (randomiseBlockSizes);
            runLatencyTests (randomiseBlockSizes);
        }
    }

private:
    struct TestContext
    {
        std::unique_ptr<TemporaryFile> tempFile;
        AudioBuffer<float> buffer;
    };
    
    std::unique_ptr<TestContext> createTestContext (std::unique_ptr<AudioNode> node, const double sampleRate, const int blockSize,
                                                    const int numChannels, const double durationInSeconds, bool randomiseBlockSizes = false)
    {
        auto context = std::make_unique<TestContext>();
        context->tempFile = std::make_unique<TemporaryFile> (".wav");
        
        // Process the node to a file
        if (auto writer = std::unique_ptr<AudioFormatWriter> (WavAudioFormat().createWriterFor (context->tempFile->getFile().createOutputStream().release(),
                                                                                                sampleRate, numChannels, 16, {}, 0)))
        {
            AudioNodeProcessor processor (std::move (node));
            processor.prepareToPlay (sampleRate, blockSize);
            
            AudioBuffer<float> buffer (1, blockSize);
            MidiBuffer midi;
            
            int numSamplesToDo = roundToInt (durationInSeconds * sampleRate);
            Random r;
            
            for (;;)
            {
                const int numThisTime = randomiseBlockSizes ? std::min (r.nextInt ({ 1, blockSize }), numSamplesToDo)
                                                            : std::min (blockSize, numSamplesToDo);
                buffer.clear();
                midi.clear();
                
                AudioBuffer<float> subSectionBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                     0, numThisTime);

                processor.process (subSectionBuffer, midi);
                
                writer->writeFromAudioSampleBuffer (subSectionBuffer, 0, subSectionBuffer.getNumSamples());
                
                numSamplesToDo -= numThisTime;
                
                if (numSamplesToDo <= 0)
                    break;
            }
            
            writer.reset();

            // Then read it back in to the buffer
            if (auto reader = std::unique_ptr<AudioFormatReader> (WavAudioFormat().createReaderFor (context->tempFile->getFile().createInputStream().release(), true)))
            {
                AudioBuffer<float> buffer (numChannels, (int) reader->lengthInSamples);
                reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, false);
                context->buffer = std::move (buffer);
                
                return context;
            }
        }

        return {};
    }
    
    void runSinTest (bool randomiseBlockSizes)
    {
        beginTest ("Sin");
        {
            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            
            auto testContext = createTestContext (std::move (sinNode), 44100.0, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;
            
            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.707f, 0.001f);
        }
    }

    void runSinCancellingTest (bool randomiseBlockSizes)
    {
        beginTest ("Sin cancelling");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0));

            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            auto invertedSinNode = std::make_unique<FunctionAudioNode> (std::move (sinNode), [] (float s) { return -s; });
            nodes.push_back (std::move (invertedSinNode));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));
            
            auto testContext = createTestContext (std::move (sumNode), 44100.0, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
        }
    }

    void runSinOctaveTest (bool randomiseBlockSizes)
    {
        beginTest ("Sin octave");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0));
            nodes.push_back (std::make_unique<SinAudioNode> (440.0));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));
            auto node = std::make_unique<FunctionAudioNode> (std::move (sumNode), [] (float s) { return s * 0.5f; });
            
            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.885f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.5f, 0.001f);
        }
    }
    
    void runSendReturnTest (bool randomiseBlockSizes)
    {
        beginTest ("Sin send/return");
        {
            // Track 1 sends a sin tone to a send and then gets muted
            auto sinLowerNode = std::make_unique<SinAudioNode> (220.0);
            auto sendNode = std::make_unique<SendAudioNode> (std::move (sinLowerNode), 1);
            auto track1Node = std::make_unique<FunctionAudioNode> (std::move (sendNode), [] (float) { return 0.0f; });

            // Track 2 has a silent source and receives input from the send
            auto sinUpperNode = std::make_unique<SinAudioNode> (440.0);
            auto silentNode = std::make_unique<FunctionAudioNode> (std::move (sinUpperNode), [] (float) { return 0.0f; });
            auto track2Node = std::make_unique<ReturnAudioNode> (std::move (silentNode), 1);

            // Track 1 & 2 then get summed together
            auto node = makeBaicSummingAudioNode ({ track1Node.release(), track2Node.release() });

            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.707f, 0.001f);
        }

        beginTest ("Sin send/return different bus#");
        {
            // This test is the same as before but uses a different bus number for the return so the output should be silent

            // Track 1 sends a sin tone to a send and then gets muted
            auto sinLowerNode = std::make_unique<SinAudioNode> (220.0);
            auto sendNode = std::make_unique<SendAudioNode> (std::move (sinLowerNode), 1);
            auto track1Node = std::make_unique<FunctionAudioNode> (std::move (sendNode), [] (float) { return 0.0f; });

            // Track 2 has a silent source and receives input from the send
            auto sinUpperNode = std::make_unique<SinAudioNode> (440.0);
            auto silentNode = std::make_unique<FunctionAudioNode> (std::move (sinUpperNode), [] (float) { return 0.0f; });
            auto track2Node = std::make_unique<ReturnAudioNode> (std::move (silentNode), 2);

            // Track 1 & 2 then get summed together
            auto node = makeBaicSummingAudioNode ({ track1Node.release(), track2Node.release() });

            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
        }
        
        beginTest ("Sin send/return non-blocking");
        {
            // Track 1 sends a sin tone to a send with a gain of 0.25
            auto sinLowerNode = std::make_unique<SinAudioNode> (220.0);
            auto attenuatedSinLowerNode = std::make_unique<FunctionAudioNode> (std::move (sinLowerNode), [] (float s) { return s * 0.25f; });
            auto track1Node = std::make_unique<SendAudioNode> (std::move (attenuatedSinLowerNode), 1);

            // Track 2 has a sin source  of gain 0.5 and receives input from the send
            auto sinUpperNode = std::make_unique<SinAudioNode> (440.0);
            auto attenuatedSinUpperNode = std::make_unique<FunctionAudioNode> (std::move (sinUpperNode), [] (float s) { return s * 0.5f; });
            auto track2Node = std::make_unique<ReturnAudioNode> (std::move (attenuatedSinUpperNode), 1);

            // Track 1 & 2 then get summed together
            auto node = makeBaicSummingAudioNode ({ track1Node.release(), track2Node.release() });
            
            auto testContext = createTestContext (std::move (node), 44100.0, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.885f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.5f, 0.001f);
        }
    }
    
    void runLatencyTests (bool randomiseBlockSizes)
    {
        beginTest ("Basic latency test cancelling sin");
        {
            /*  Has two sin nodes at the same frequency and delays one of them by half a period
                so they should cancel out completely.
                This uses the test sample rate to determine the sin frequency to avoid rounding errors.
            */
            const double sampleRate = 44100.0;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = roundToInt (numSamplesPerCycle / 2.0);

            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (sinFrequency));

            auto sinNode = std::make_unique<SinAudioNode> (sinFrequency);
            auto latencySinNode = std::make_unique<LatencyAudioNode> (std::move (sinNode), numLatencySamples);
            nodes.push_back (std::move (latencySinNode));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(),
                                              buffer.getNumChannels(),
                                              numLatencySamples, buffer.getNumSamples() - numLatencySamples);
            
            expectWithinAbsoluteError (trimmedBuffer.getMagnitude (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (trimmedBuffer.getRMSLevel (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
        }
        
        beginTest ("Basic latency test doubling sin");
        {
            /*  This is the same test as before, two sin waves with one delayed but now the second one is compensated for.
                This has two implications:
                 1. There will be a half period of silence at the start of the output
                 2. Instead of cancelling, the sins will now constructively interfere, doubling the magnitude
            */
            const double sampleRate = 44100.0;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = roundToInt (numSamplesPerCycle / 2.0);

            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (makeGainNode (std::make_unique<SinAudioNode> (sinFrequency), 0.5f));

            auto sinNode = makeGainNode (std::make_unique<SinAudioNode> (sinFrequency), 0.5f);
            auto latencySinNode = std::make_unique<LatencyAudioNode> (std::move (sinNode), numLatencySamples);
            nodes.push_back (std::move (latencySinNode));

            auto sumNode = std::make_unique<SummingAudioNode> (std::move (nodes));

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            // Start of buffer which should be silent
            {
                AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                  0, numLatencySamples);
                expectWithinAbsoluteError (trimmedBuffer.getMagnitude (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
                expectWithinAbsoluteError (trimmedBuffer.getRMSLevel (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
            }

            // Part of buffer after latency which should be all sin +-1.0
            {
                AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                  numLatencySamples, buffer.getNumSamples() - numLatencySamples);
                expectWithinAbsoluteError (trimmedBuffer.getMagnitude (0, 0, trimmedBuffer.getNumSamples()), 1.0f, 0.001f);
                expectWithinAbsoluteError (trimmedBuffer.getRMSLevel (0, 0, trimmedBuffer.getNumSamples()), 0.707f, 0.001f);
            }
        }

        beginTest ("Basic latency test doubling sin multiple sized blocks");
        {
            /*  This is the same test as before, two sin waves with one delayed and latency compensated
                but this time the processing happens in different sized blocks
            */
            const double sampleRate = 44100.0;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = roundToInt (numSamplesPerCycle / 2.0);

            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (makeGainNode (std::make_unique<SinAudioNode> (sinFrequency), 0.5f));

            auto sinNode = makeGainNode (std::make_unique<SinAudioNode> (sinFrequency), 0.5f);
            auto latencySinNode = std::make_unique<LatencyAudioNode> (std::move (sinNode), numLatencySamples);
            nodes.push_back (std::move (latencySinNode));

            auto sumNode = std::make_unique<SummingAudioNode> (std::move (nodes));

            auto testContext = createTestContext (std::move (sumNode), sampleRate, 512, 1, 5.0, randomiseBlockSizes);
            auto& buffer = testContext->buffer;

            // Start of buffer which should be silent
            {
                AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                  0, numLatencySamples);
                expectWithinAbsoluteError (trimmedBuffer.getMagnitude (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
                expectWithinAbsoluteError (trimmedBuffer.getRMSLevel (0, 0, trimmedBuffer.getNumSamples()), 0.0f, 0.001f);
            }

            // Part of buffer after latency which should be all sin +-1.0
            {
                AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                  numLatencySamples, buffer.getNumSamples() - numLatencySamples);
                expectWithinAbsoluteError (trimmedBuffer.getMagnitude (0, 0, trimmedBuffer.getNumSamples()), 1.0f, 0.001f);
                expectWithinAbsoluteError (trimmedBuffer.getRMSLevel (0, 0, trimmedBuffer.getNumSamples()), 0.707f, 0.001f);
            }
        }
    }
};

static AudioNodeTests audioNodeTests;
