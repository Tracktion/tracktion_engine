/*
  ==============================================================================

    tests_AudioNode.cpp
    Created: 15 Apr 2020 10:44:09am
    Author:  David Rowland

  ==============================================================================
*/

#include "AudioNodeProcessor.h"
#include "UtilityNodes.h"
#include "TestAudioNodes.h"


//==============================================================================
namespace test_utilities
{
    /** Creates a random MidiMessageSequence sequence. */
    MidiMessageSequence createRandomMidiMessageSequence (double durationSeconds, Random r)
    {
        juce::MidiMessageSequence sequence;
        int noteNumber = -1;
        
        for (double time = 0.0; time < durationSeconds; time += r.nextDouble() * 0.5 + 0.1)
        {
            if (noteNumber != -1)
            {
                sequence.addEvent (MidiMessage::noteOff (1, noteNumber), time);
                noteNumber = -1;
            }
            else
            {
                noteNumber = r.nextInt ({ 1, 127 });
                sequence.addEvent (MidiMessage::noteOn (1, noteNumber, 1.0f), time);
            }
        }
        
        return sequence;
    }

    /** Creates a MidiMessageSequence from a MidiBuffer. */
    static MidiMessageSequence createMidiMessageSequence (const juce::MidiBuffer& buffer, double sampleRate)
    {
        MidiMessageSequence sequence;
        
        for (MidiBuffer::Iterator iter (buffer);;)
        {
            MidiMessage result;
            int samplePosition = 0;
            
            if (! iter.getNextEvent (result, samplePosition))
                break;
            
            const double time = samplePosition / sampleRate;
            sequence.addEvent (result, time);
        }
        
        return sequence;
    }

    static void fillBufferWithSinData (AudioBuffer<float>& buffer)
    {
        const float incremement = MathConstants<float>::twoPi / buffer.getNumSamples();
        float* sample = buffer.getWritePointer (0);
        
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            *sample++ = std::sin (incremement * i);
        
        for (int i = 1; i < buffer.getNumChannels(); ++i)
            buffer.copyFrom (i, 0, buffer, 0, 0, buffer.getNumSamples());
    }

    /** Logs a MidiMessageSequence. */
    void logMidiMessageSequence (juce::UnitTest& ut, const juce::MidiMessageSequence& seq)
    {
        for (int i = 0; i < seq.getNumEvents(); ++i)
            ut.logMessage (seq.getEventPointer (i)->message.getDescription());
    }

    /** Writes an audio buffer to a file. */
    void writeToFile (File file, const AudioBuffer<float>& buffer, double sampleRate)
    {
        if (auto writer = std::unique_ptr<AudioFormatWriter> (WavAudioFormat().createWriterFor (file.createOutputStream().release(),
                                                                                                sampleRate, (uint32_t) buffer.getNumChannels(), 16, {}, 0)))
        {
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        }
    }

    /** Writes an audio block to a file. */
    void writeToFile (File file, const juce::dsp::AudioBlock<float>& block, double sampleRate)
    {
        const int numChannels = (int) block.getNumChannels();
        float* chans[32] = {};

        for (int i = 0; i < numChannels; ++i)
            chans[i] = block.getChannelPointer ((size_t) i);

        const juce::AudioBuffer<float> buffer (chans, numChannels, (int) block.getNumSamples());
        writeToFile (file, buffer, sampleRate);
    }

    //==============================================================================
    /** Compares two MidiMessageSequences and expects them to be equal. */
    static void expectMidiMessageSequence (juce::UnitTest& ut, const juce::MidiMessageSequence& seq1, const juce::MidiMessageSequence& seq2)
    {
        ut.expectEquals (seq1.getNumEvents(), seq2.getNumEvents(), "Num MIDI events not equal");
        
        if (seq1.getNumEvents() != seq2.getNumEvents())
            return;
        
        bool sequencesTheSame = true;
        
        for (int i = 0; i < seq1.getNumEvents(); ++i)
        {
            auto event1 = seq1.getEventPointer (i);
            auto event2 = seq2.getEventPointer (i);
            
            auto desc1 = event1->message.getDescription();
            auto desc2 = event2->message.getDescription();

            if (desc1 != desc2)
            {
                ut.logMessage (String ("Event at index 123 is:\n\tseq1: XXX\n\tseq2 is: YYY")
                                .replace ("123", String (i)).replace ("XXX", desc1).replace ("YYY", desc2));
                sequencesTheSame = false;
            }
        }
        
        ut.expect (sequencesTheSame, "MIDI sequence contents not equal");
    }

    /** Compares a MidiBuffer and a MidiMessageSequence and expects them to be equal. */
    static void expectMidiBuffer (juce::UnitTest& ut, const juce::MidiBuffer& buffer, double sampleRate, const juce::MidiMessageSequence& seq)
    {
        expectMidiMessageSequence (ut, createMidiMessageSequence (buffer, sampleRate), seq);
    }

    /** Expects a specific magnitude and RMS from an AudioBuffer's channel. */
    static void expectAudioBuffer (juce::UnitTest& ut, const AudioBuffer<float>& buffer, int channel, float mag, float rms)
    {
        ut.expectWithinAbsoluteError (buffer.getMagnitude (channel, 0, buffer.getNumSamples()), mag, 0.001f);
        ut.expectWithinAbsoluteError (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()), rms, 0.001f);
    }

    /** Splits a buffer in to two and expects a specific magnitude and RMS from each half AudioBuffer. */
    static void expectAudioBuffer (juce::UnitTest& ut, AudioBuffer<float>& buffer, int channel, int numSampleToSplitAt,
                                   float mag1, float rms1, float mag2, float rms2)
    {
        {
            AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                              0, numSampleToSplitAt);
            expectAudioBuffer (ut, trimmedBuffer, channel, mag1, rms1);
        }
        
        {
            AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(),
                                              buffer.getNumChannels(),
                                              numSampleToSplitAt, buffer.getNumSamples() - numSampleToSplitAt);
            expectAudioBuffer (ut, trimmedBuffer, channel, mag2, rms2);
        }
    }
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
    
    struct TestSetup
    {
        double sampleRate = 44100.0;
        int blockSize = 512;
        bool randomiseBlockSizes = false;
        Random random;
    };
    
    void runTest() override
    {
        for (double sampleRate : { 44100.0, 48000.0, 96000.0 })
        {
            for (int blockSize : { 64, 256, 512, 1024 })
            {
                for (bool randomiseBlockSizes : { false, true })
                {
                    TestSetup setup { sampleRate, blockSize, randomiseBlockSizes, getRandom() };
                    logMessage (String ("Test setup: sample rate SR, block size BS, random blocks RND")
                                .replace ("SR", String (sampleRate))
                                .replace ("BS", String (blockSize))
                                .replace ("RND", randomiseBlockSizes ? "Y" : "N"));

                    // Mono tests
                    runSinTests (setup);
                    runSinCancellingTests (setup);
                    runSinOctaveTests (setup);
                    runSendReturnTests (setup);
                    runLatencyTests (setup);

                    // MIDI tests
                    runMidiTests (setup);

                    // Multi channel tests
                    runStereoTests (setup);

                    // Rack tests
                    runRackTests (setup);
                    runRackAudioInputTests (setup);
                }
            }
        }
    }

private:
    //==============================================================================
    struct TestContext
    {
        std::unique_ptr<TemporaryFile> tempFile;
        AudioBuffer<float> buffer;
        MidiBuffer midi;
    };

    std::unique_ptr<TestContext> createBasicTestContext (std::unique_ptr<AudioNode> node, const TestSetup ts,
                                                         const int numChannels, const double durationInSeconds)
    {
        auto processor = std::make_unique<AudioNodeProcessor> (std::move (node));
        return createTestContext (std::move (processor), ts, numChannels, durationInSeconds);
    }
    
    template<typename AudioNodeProcessorType>
    std::unique_ptr<TestContext> createTestContext (std::unique_ptr<AudioNodeProcessorType> processor, TestSetup ts,
                                                    const int numChannels, const double durationInSeconds)
    {
        auto context = std::make_unique<TestContext>();
        context->tempFile = std::make_unique<TemporaryFile> (".wav");
        
        // Process the node to a file
        if (auto writer = std::unique_ptr<AudioFormatWriter> (WavAudioFormat().createWriterFor (context->tempFile->getFile().createOutputStream().release(),
                                                                                                ts.sampleRate, (uint32_t) numChannels, 16, {}, 0)))
        {
            processor->prepareToPlay (ts.sampleRate, ts.blockSize);
            
            AudioBuffer<float> buffer (numChannels, ts.blockSize);
            MidiBuffer midi;
            
            int numSamplesToDo = roundToInt (durationInSeconds * ts.sampleRate);
            int numSamplesDone = 0;
            
            for (;;)
            {
                const int numThisTime = ts.randomiseBlockSizes ? std::min (ts.random.nextInt ({ 1, ts.blockSize }), numSamplesToDo)
                                                               : std::min (ts.blockSize, numSamplesToDo);
                buffer.clear();
                midi.clear();
                
                AudioBuffer<float> subSectionBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                     0, numThisTime);

                processor->process ({ { { subSectionBuffer }, midi } });
                
                writer->writeFromAudioSampleBuffer (subSectionBuffer, 0, subSectionBuffer.getNumSamples());
                context->midi.addEvents (midi, 0, numThisTime, numSamplesDone);
                
                numSamplesToDo -= numThisTime;
                numSamplesDone += numThisTime;
                
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
    
    //==============================================================================
    //==============================================================================
    void runSinTests (TestSetup testSetup)
    {
        beginTest ("Sin");
        {
            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            
            auto testContext = createBasicTestContext (std::move (sinNode), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
        }
    }

    void runSinCancellingTests (TestSetup testSetup)
    {
        beginTest ("Sin cancelling");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0));

            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            auto invertedSinNode = std::make_unique<FunctionAudioNode> (std::move (sinNode), [] (float s) { return -s; });
            nodes.push_back (std::move (invertedSinNode));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));
            
            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.0f, 0.0f);
        }
    }

    void runSinOctaveTests (TestSetup testSetup)
    {
        beginTest ("Sin octave");
        {
            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (220.0));
            nodes.push_back (std::make_unique<SinAudioNode> (440.0));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));
            auto node = std::make_unique<FunctionAudioNode> (std::move (sumNode), [] (float s) { return s * 0.5f; });
            
            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.885f, 0.5f);
        }
    }
    
    void runSendReturnTests (TestSetup testSetup)
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

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 1.0f, 0.707f);
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

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.0f, 0.0f);
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
            
            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, 0.885f, 0.5f);
        }
    }
    
    void runLatencyTests (TestSetup testSetup)
    {
        beginTest ("Basic latency test cancelling sin");
        {
            /*  Has two sin nodes at the same frequency and delays one of them by half a period
                so they should cancel out completely.
                This uses the test sample rate to determine the sin frequency to avoid rounding errors.
            */
            const double sampleRate = testSetup.sampleRate;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = roundToInt (numSamplesPerCycle / 2.0);

            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (std::make_unique<SinAudioNode> (sinFrequency));

            auto sinNode = std::make_unique<SinAudioNode> (sinFrequency);
            auto latencySinNode = std::make_unique<LatencyAudioNode> (std::move (sinNode), numLatencySamples);
            nodes.push_back (std::move (latencySinNode));

            auto sumNode = std::make_unique<BasicSummingAudioNode> (std::move (nodes));

            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, 5.0);

            // Start of buffer is +-1, after latency comp kicks in, the second half will be silent
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 1.0f, 0.707f, 0.0f, 0.0f);
        }
        
        beginTest ("Basic latency test doubling sin");
        {
            /*  This is the same test as before, two sin waves with one delayed but now the second one is compensated for.
                This has two implications:
                 1. There will be a half period of silence at the start of the output
                 2. Instead of cancelling, the sins will now constructively interfere, doubling the magnitude
            */
            const double sampleRate = testSetup.sampleRate;
            const double sinFrequency = sampleRate / 100.0;
            const double numSamplesPerCycle = sampleRate / sinFrequency;
            const int numLatencySamples = roundToInt (numSamplesPerCycle / 2.0);

            std::vector<std::unique_ptr<AudioNode>> nodes;
            nodes.push_back (makeGainNode (makeAudioNode<SinAudioNode> (sinFrequency), 0.5f));

            auto sinNode = makeGainNode (makeAudioNode<SinAudioNode> (sinFrequency), 0.5f);
            auto latencySinNode = makeAudioNode<LatencyAudioNode> (std::move (sinNode), numLatencySamples);
            nodes.push_back (std::move (latencySinNode));

            auto sumNode = makeAudioNode<SummingAudioNode> (std::move (nodes));

            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, 5.0);
            // Start of buffer which should be silent
            // Part of buffer after latency which should be all sin +-1.0
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }

        beginTest ("Send/return with latency");
        {
            /*  This has a sin input to a latency node leading to a send.
                The return branch also has a sin input. The latency should be compensated on the return node correctly.
            */
            const double sinFrequency = testSetup.sampleRate / 100.0;
            const double numSamplesPerCycle = testSetup.sampleRate / sinFrequency;
            const int numLatencySamples = roundToInt (numSamplesPerCycle / 2.0);

            auto track1 = makeAudioNode<SinAudioNode> (sinFrequency);
            track1 = makeAudioNode<LatencyAudioNode> (std::move (track1), numLatencySamples);
            track1 = makeGainNode (std::move (track1), 0.5f);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), 1);
            track1 = makeGainNode (std::move (track1), 0.0f);

            auto track2 = makeAudioNode<SinAudioNode> (sinFrequency);
            track2 = makeGainNode (std::move (track2), 0.5f);
            track2 = makeAudioNode<ReturnAudioNode> (std::move (track2), 1);
            
            auto node = makeSummingAudioNode ({ track1.release(), track2.release() });

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);

            // Start of buffer which should be silent
            // Part of buffer after latency which should be all sin +-1.0
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }

        beginTest ("Multiple send/return with latency");
        {
            /*  This two tracks with sin input to a latency node leading to a send. The latency is different on each branch.
                The latency should be compensated on the return node correctly.
            */
            const double sinFrequency = testSetup.sampleRate / 100.0;
            const double numSamplesPerCycle = testSetup.sampleRate / sinFrequency;
            const int numLatencySamples = roundToInt (numSamplesPerCycle / 2.0);

            auto track1 = makeAudioNode<SinAudioNode> (sinFrequency);
            track1 = makeAudioNode<LatencyAudioNode> (std::move (track1), numLatencySamples);
            track1 = makeGainNode (std::move (track1), 0.5f);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), 1);
            track1 = makeGainNode (std::move (track1), 0.0f);

            auto track2 = makeAudioNode<SinAudioNode> (sinFrequency);
            track2 = makeAudioNode<LatencyAudioNode> (std::move (track2), numLatencySamples * 2);
            track2 = makeGainNode (std::move (track2), 0.5f);
            track2 = makeAudioNode<SendAudioNode> (std::move (track2), 1);
            track2 = makeGainNode (std::move (track2), 0.0f);

            auto track3 = makeAudioNode<SinAudioNode> (sinFrequency);
            track3 = makeGainNode (std::move (track3), 0.0f);
            track3 = makeAudioNode<ReturnAudioNode> (std::move (track3), 1);
            
            auto node = makeSummingAudioNode ({ track1.release(), track2.release(), track3.release() });

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);

            // Start of buffer which should be silent
            // Part of buffer after latency which should be all sin +-1.0
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }
        
        beginTest ("Send, send/return with two stage latency");
        {
            /*  This has a sin input to a latency node leading to a another latency block and another send on a different bus.
                There are then two other tracks that receive each of the send nodes.
                The latency should be compensated for and the output a mag 1 sin.
            */
            const double sinFrequency = testSetup.sampleRate / 100.0;
            const double numSamplesPerCycle = testSetup.sampleRate / sinFrequency;
            const int numLatencySamples = roundToInt (numSamplesPerCycle / 2.0);

            auto track1 = makeAudioNode<SinAudioNode> (sinFrequency);
            track1 = makeAudioNode<LatencyAudioNode> (std::move (track1), numLatencySamples);
            track1 = makeGainNode (std::move (track1), 0.5f);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), 1);
            track1 = makeAudioNode<LatencyAudioNode> (std::move (track1), numLatencySamples);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), 2);
            track1 = makeGainNode (std::move (track1), 0.0f);

            auto track2 = makeAudioNode<SilentAudioNode> (1);
            track2 = makeAudioNode<ReturnAudioNode> (std::move (track2), 1);

            auto track3 = makeAudioNode<SilentAudioNode> (1);
            track3 = makeAudioNode<ReturnAudioNode> (std::move (track3), 2);

            auto node = makeSummingAudioNode ({ track1.release(), track2.release(), track3.release() });

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);

            // Start of buffer which should be silent
            // Part of buffer after latency which should be all sin +-1.0
            test_utilities::expectAudioBuffer (*this, testContext->buffer, 0, numLatencySamples, 0.0f, 0.0f, 1.0f, 0.707f);
        }
    }
        
    void runMidiTests (TestSetup testSetup)
    {
        const double sampleRate = 44100.0;
        const double duration = 5.0;
        const auto sequence = test_utilities::createRandomMidiMessageSequence (duration, testSetup.random);
        
        beginTest ("Basic MIDI");
        {
            auto node = std::make_unique<MidiAudioNode> (sequence);
            
            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, duration);

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
        }
        
        beginTest ("Delayed MIDI");
        {
            expectGreaterThan (sequence.getNumEvents(), 0);
            
            const int latencyNumSamples = roundToInt (sampleRate / 100.0);
            const double delayedTime = latencyNumSamples / sampleRate;
            auto midiNode = std::make_unique<MidiAudioNode> (sequence);
            auto delayedNode = std::make_unique<LatencyAudioNode> (std::move (midiNode), latencyNumSamples);

            auto testContext = createBasicTestContext (std::move (delayedNode), testSetup, 1, duration);
            
            auto extectedSequence = sequence;
            extectedSequence.addTimeToMessages (delayedTime);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, extectedSequence);
        }

        beginTest ("Compensated MIDI");
        {
            // This test has a sin node being delayed by a block which then gets mixed with a non-deleayed MIDI stream
            // The MIDI stream should be delayed by the same amount as the sin stream
            expectGreaterThan (sequence.getNumEvents(), 0);
            
            const int latencyNumSamples = roundToInt (sampleRate / 100.0);
            const double delayedTime = latencyNumSamples / sampleRate;

            auto sinNode = std::make_unique<SinAudioNode> (220.0);
            auto delayedNode = std::make_unique<LatencyAudioNode> (std::move (sinNode), latencyNumSamples);
            
            auto midiNode = std::make_unique<MidiAudioNode> (sequence);
            auto summedNode = makeSummingAudioNode ({ delayedNode.release(), midiNode.release() });

            auto testContext = createBasicTestContext (std::move (summedNode), testSetup, 1, duration);
            
            auto extectedSequence = sequence;
            extectedSequence.addTimeToMessages (delayedTime);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, extectedSequence);
        }

        beginTest ("Send/return MIDI");
        {
            // Test that sends MIDI from one branch of a node to another and mutes the original
            const int busNum = 1;
            
            auto track1 = makeAudioNode<MidiAudioNode> (sequence);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), busNum);
            track1 = makeAudioNode<FunctionAudioNode> (std::move (track1), [] (float) { return 0.0f; });

            auto track2 = makeAudioNode<ReturnAudioNode> (makeAudioNode<SinAudioNode> (220.0), busNum);
            
            auto sumNode = makeSummingAudioNode ({ track1.release(), track2.release() });

            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, duration);

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
        }
        
        beginTest ("Send/return MIDI passthrough");
        {
            // Test that sends MIDI from one branch of a node to another and mutes the return path
            const int busNum = 1;
            
            auto track1 = makeAudioNode<MidiAudioNode> (sequence);
            track1 = makeAudioNode<SendAudioNode> (std::move (track1), busNum);

            auto track2 = makeAudioNode<ReturnAudioNode> (makeAudioNode<SinAudioNode> (220.0), busNum);
            track2 = makeAudioNode<FunctionAudioNode> (std::move (track2), [] (float) { return 0.0f; });

            auto sumNode = makeSummingAudioNode ({ track1.release(), track2.release() });

            auto testContext = createBasicTestContext (std::move (sumNode), testSetup, 1, duration);

            expectGreaterThan (sequence.getNumEvents(), 0);
            test_utilities::expectMidiBuffer (*this, testContext->midi, sampleRate, sequence);
        }
    }
    
    void runStereoTests (TestSetup testSetup)
    {
        beginTest ("Stereo sin");
        {
            auto node = makeAudioNode<SinAudioNode> (220.0, 2);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 2, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.707f, 0.001f);
        }

        beginTest ("Stereo sin");
        {
            // Two mono sin nodes summed to L/R stereo
            auto leftSin = makeAudioNode<SinAudioNode> (220.0, 1);
            auto rightSin = makeAudioNode<SinAudioNode> (220.0, 1);

            std::vector<std::pair<int, int>> channelMap;
            channelMap.push_back ({ 0, 1 });
            auto rightRemapped = makeAudioNode<ChannelMappingAudioNode> (std::move (rightSin), channelMap, true);

            auto node = makeSummingAudioNode ({ leftSin.release(), rightRemapped.release() });

            expectEquals (node->getAudioNodeProperties().numberOfChannels, 2);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 2, 5.0);
            auto& buffer = testContext->buffer;

            for (int channel : { 0, 1 })
            {
                expectWithinAbsoluteError (buffer.getMagnitude (channel, 0, buffer.getNumSamples()), 1.0f, 0.001f);
                expectWithinAbsoluteError (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()), 0.707f, 0.001f);
            }
        }

        beginTest ("Stereo sin summed to mono");
        {
            // A stero sin node at 0.5 is summed to mono to produce a 1.0 mono sin
            auto node = makeAudioNode<SinAudioNode> (220.0, 2);
            node = makeGainAudioNode (std::move (node), 0.5f);

            // Merge channe 1 with channel 2
            std::vector<std::pair<int, int>> channelMap;
            channelMap.push_back ({ 0, 0 });
            channelMap.push_back ({ 1, 0 });
            node = makeAudioNode<ChannelMappingAudioNode> (std::move (node), channelMap, true);

            expectEquals (node->getAudioNodeProperties().numberOfChannels, 1);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 1.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.707f, 0.001f);
        }
        
        beginTest ("Twin mono sin summed to mono cancelling");
        {
            // L/R sin with inverted phase that cancel
            auto leftNode = makeAudioNode<SinAudioNode> (220.0, 1);

            auto rightNode = makeAudioNode<SinAudioNode> (220.0, 1);
            rightNode = makeAudioNode<FunctionAudioNode> (std::move (rightNode), [] (float s) { return s * -1.0f; });
            rightNode = makeAudioNode<ChannelMappingAudioNode> (std::move (rightNode), makeChannelMap ({ { 0, 1 } }), true);

            auto sumNode = makeSummingAudioNode ({ leftNode.release(), rightNode.release() });

            // Merge channe 1 with channel 2
            auto node = makeAudioNode<ChannelMappingAudioNode> (std::move (sumNode), makeChannelMap ({ { 0, 0 }, { 1, 0 } }), true);

            expectEquals (node->getAudioNodeProperties().numberOfChannels, 1);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 1, 5.0);
            auto& buffer = testContext->buffer;

            expectWithinAbsoluteError (buffer.getMagnitude (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
            expectWithinAbsoluteError (buffer.getRMSLevel (0, 0, buffer.getNumSamples()), 0.0f, 0.001f);
        }
        
        beginTest ("Mono sin duplicated to 6 channel");
        {
            // Create a single mono sin and then copy that to 6 channels
            auto node = makeAudioNode<SinAudioNode> (220.0, 1);
            node = makeAudioNode<ChannelMappingAudioNode> (std::move (node),
                                                           makeChannelMap ({ { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 }, { 0, 4 }, { 0, 5 } }),
                                                           true);

            expectEquals (node->getAudioNodeProperties().numberOfChannels, 6);

            auto testContext = createBasicTestContext (std::move (node), testSetup, 6, 5.0);
            auto& buffer = testContext->buffer;

            for (int channel : { 0, 1, 2, 3, 4, 5 })
            {
                expectWithinAbsoluteError (buffer.getMagnitude (channel, 0, buffer.getNumSamples()), 1.0f, 0.001f);
                expectWithinAbsoluteError (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()), 0.707f, 0.001f);
            }
        }
    }
    
    // Defined in RackNode.cpp
    void runRackTests (TestSetup);
    void runRackAudioInputTests (TestSetup);
};

static AudioNodeTests audioNodeTests;

#include "RackNode.cpp"
