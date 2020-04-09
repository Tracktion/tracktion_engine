/*
  ==============================================================================

    RackNode.cpp
    Created: 8 Apr 2020 3:41:40pm
    Author:  David Rowland

  ==============================================================================
*/

#include "RackNode.h"

/**
    Tests:
    1. Build a node that generates a sin wave
    2. Add another node that generates a sin wave an octave higher
    3. Make two sin waves, add latency of the period to one of these, the output should be silent
*/

/**
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

//==============================================================================
//==============================================================================
class AudioNode
{
public:
    AudioNode() = default;
    virtual ~AudioNode() = default;
    
    virtual tracktion_engine::AudioNodeProperties getAudioNodeProperties() = 0;

    virtual bool isReadyToProcess() = 0;
    
    virtual void prepareToPlay (double sampleRate, int blockSize) = 0;
    virtual void process (AudioBuffer<float>&, MidiBuffer&) = 0;
    
private:
};


//==============================================================================
//==============================================================================
class SinAudioNode : public AudioNode
{
public:
    SinAudioNode (double frequency)
    {
        osc.setFrequency (frequency);
    }
    
    tracktion_engine::AudioNodeProperties getAudioNodeProperties() override
    {
        tracktion_engine::AudioNodeProperties props;
        props.hasAudio = true;
        props.hasMidi = false;
        props.numberOfChannels = 1;
        
        return props;
    }
    
    bool isReadyToProcess() override
    {
        return true;
    }
    
    void prepareToPlay (double sampleRate, int blockSize) override
    {
        osc.prepare ({ double (sampleRate), uint32 (blockSize), 1 });
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
class SummingAudioNode : public AudioNode
{
public:
    SummingAudioNode (std::vector<std::unique_ptr<AudioNode>> inputs)
        : nodes (std::move (inputs))
    {
    }
    
    tracktion_engine::AudioNodeProperties getAudioNodeProperties() override
    {
        tracktion_engine::AudioNodeProperties props;
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

        tempBuffer.setSize (props.numberOfChannels, 32);
        
        return props;
    }

    bool isReadyToProcess() override
    {
        for (auto& node : nodes)
            if (! node->isReadyToProcess())
                return false;
        
        return true;
    }
    
    void prepareToPlay (double sampleRate, int blockSize) override
    {
        for (auto& node : nodes)
            node->prepareToPlay (sampleRate, blockSize);
        
        tempBuffer.setSize (tempBuffer.getNumChannels(), blockSize);
    }
    
    void process (AudioBuffer<float>& dest, MidiBuffer& midi) override
    {
        jassert (tempBuffer.getNumChannels() <= dest.getNumChannels()); // numChannels can be more than our nodes have
        jassert (dest.getNumSamples() <= tempBuffer.getNumSamples());   // numSamples must be no greater than we have been prepared for
        
        const int numSamples = dest.getNumSamples();
        
        for (auto& node : nodes)
        {
            tempBuffer.clear();
            tempMidi.clear();
            
            AudioBuffer<float> bufferToFill (tempBuffer.getArrayOfWritePointers(), tempBuffer.getNumChannels(), numSamples);
            node->process (bufferToFill, tempMidi);
            
            for (int i = 0; i < dest.getNumChannels(); ++i)
                dest.addFrom (i, 0, bufferToFill, i, 0, numSamples);

            midi.addEvents (tempMidi, 0, dest.getNumSamples(), 0);
        }
    }

private:
    std::vector<std::unique_ptr<AudioNode>> nodes;
    AudioBuffer<float> tempBuffer;
    MidiBuffer tempMidi;
};


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
    
    tracktion_engine::AudioNodeProperties getAudioNodeProperties() override
    {
        return node->getAudioNodeProperties();
    }
    
    bool isReadyToProcess() override
    {
        return node->isReadyToProcess();
    }
    
    void prepareToPlay (double sampleRate, int blockSize) override
    {
        node->prepareToPlay (sampleRate, blockSize);
    }
    
    void process (AudioBuffer<float>& buffer, MidiBuffer& midi) override
    {
        node->process (buffer, midi);

        int numSamples = buffer.getNumSamples();

        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            float* samples = buffer.getWritePointer (c);
            
            for (int i = 0; i < numSamples; ++i)
                samples[i] = function (samples[i]);
        }
    }
    
private:
    std::unique_ptr<AudioNode> node;
    std::function<float (float)> function;
};


//==============================================================================
//==============================================================================
class AudioNodeProcessor
{
public:
    AudioNodeProcessor (std::unique_ptr<AudioNode> nodeToProcess)
        : node (std::move (nodeToProcess))
    {
        node->getAudioNodeProperties();
    }

    void prepareToPlay (double sampleRate, int blockSize)
    {
        node->prepareToPlay (sampleRate, blockSize);
    }

    void process (AudioBuffer<float>& audio, MidiBuffer& midi)
    {
        node->process (audio, midi);
    }
    
private:
    std::unique_ptr<AudioNode> node;
    std::vector<AudioNode*> allNodes;
};

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
        runSinTest();
        runSinCancellingTest();
        runSinOctaveTest();
    }

private:
    struct TestContext
    {
        std::unique_ptr<TemporaryFile> tempFile;
        AudioBuffer<float> buffer;
    };
    
    std::unique_ptr<TestContext> createTestContext (std::unique_ptr<AudioNode> node, double sampleRate, int blockSize,
                                                    int numChannels, double durationInSeconds)
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
            
            for (;;)
            {
                const int numThisTime = std::min (blockSize, numSamplesToDo);
                
                buffer.clear();
                midi.clear();
                
                processor.process (buffer, midi);
                
                writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
                
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
    
    void runSinTest()
    {
        beginTest ("Sin");
        {
            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            
            auto testContext = createTestContext (std::move (sinNode),
                                                  44100.0, 512,
                                                  1, 5.0);
            auto& buffer = testContext->buffer;
            
            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.707f, 0.001f);
        }
    }

    void runSinCancellingTest()
    {
        beginTest ("Sin cancelling");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0));

            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            auto invertedSinNode = std::make_unique<FunctionAudioNode> (std::move (sinNode), [] (float s) { return -s; });
            nodes.push_back (std::move (invertedSinNode));

            auto sumNode = std::make_unique<SummingAudioNode> (std::move (nodes));
            
            auto testContext = createTestContext (std::move (sumNode),
                                                  44100.0, 512,
                                                  1, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
        }
    }

    void runSinOctaveTest()
    {
        beginTest ("Sin octave");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0));
            nodes.push_back (std::make_unique<SinAudioNode> (440.0));

            auto sumNode = std::make_unique<SummingAudioNode> (std::move (nodes));
            auto node = std::make_unique<FunctionAudioNode> (std::move (sumNode), [] (float s) { return s * 0.5f; });
            
            auto testContext = createTestContext (std::move (node),
                                                  44100.0, 512,
                                                  1, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.885f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.5f, 0.001f);
        }
    }
};

static AudioNodeTests audioNodeTests;
