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
namespace test_utilities
{
    /** Creates a random MidiMessageSequence sequence. */
    static inline juce::MidiMessageSequence createRandomMidiMessageSequence (double durationSeconds, juce::Random r)
    {
        juce::MidiMessageSequence sequence;
        int noteNumber = -1;
        
        for (double time = 0.0; time < durationSeconds; time += r.nextDouble() * 0.5 + 0.1)
        {
            if (noteNumber != -1)
            {
                sequence.addEvent (juce::MidiMessage::noteOff (1, noteNumber), time);
                noteNumber = -1;
            }
            else
            {
                noteNumber = r.nextInt ({ 1, 127 });
                sequence.addEvent (juce::MidiMessage::noteOn (1, noteNumber, 1.0f), time);
            }
        }
        
        return sequence;
    }

    /** Creates a MidiMessageSequence from a MidiBuffer. */
    static inline juce::MidiMessageSequence createMidiMessageSequence (const juce::MidiBuffer& buffer, double sampleRate)
    {
        juce::MidiMessageSequence sequence;
        
        for (juce::MidiBuffer::Iterator iter (buffer);;)
        {
            juce::MidiMessage result;
            int samplePosition = 0;
            
            if (! iter.getNextEvent (result, samplePosition))
                break;
            
            const double time = samplePosition / sampleRate;
            sequence.addEvent (result, time);
        }
        
        return sequence;
    }

    static inline void fillBufferWithSinData (juce::AudioBuffer<float>& buffer)
    {
        const float incremement = juce::MathConstants<float>::twoPi / buffer.getNumSamples();
        float* sample = buffer.getWritePointer (0);
        
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            *sample++ = std::sin (incremement * i);
        
        for (int i = 1; i < buffer.getNumChannels(); ++i)
            buffer.copyFrom (i, 0, buffer, 0, 0, buffer.getNumSamples());
    }

    /** Logs a MidiMessageSequence. */
    static inline void logMidiMessageSequence (juce::UnitTest& ut, const juce::MidiMessageSequence& seq)
    {
        for (int i = 0; i < seq.getNumEvents(); ++i)
            ut.logMessage (seq.getEventPointer (i)->message.getDescription());
    }

    /** Logs a MidiBuffer. */
    static inline void dgbMidiBuffer (const juce::MidiBuffer& buffer)
    {
        for (juce::MidiBuffer::Iterator iter (buffer);;)
        {
            juce::MidiMessage result;
            int samplePosition = 0;

            if (! iter.getNextEvent (result, samplePosition))
                break;

            DBG(result.getDescription());
        }
    }

    /** Writes an audio buffer to a file. */
    static inline void writeToFile (juce::File file, const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (juce::WavAudioFormat().createWriterFor (file.createOutputStream().release(),
                                                                                                            sampleRate, (uint32_t) buffer.getNumChannels(), 16, {}, 0)))
        {
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        }
    }

    /** Creates an AudioBuffer from an AudioBlock. */
    static inline juce::AudioBuffer<float> createAudioBuffer (const juce::dsp::AudioBlock<float>& block)
    {
        const int numChannels = (int) block.getNumChannels();
        float* chans[128] = {};

        for (int i = 0; i < numChannels; ++i)
            chans[i] = block.getChannelPointer ((size_t) i);

        return juce::AudioBuffer<float> (chans, numChannels, (int) block.getNumSamples());
    }

    /** Writes an audio block to a file. */
    static inline void writeToFile (juce::File file, const juce::dsp::AudioBlock<float>& block, double sampleRate)
    {
        writeToFile (file, createAudioBuffer (block), sampleRate);
    }

    /** Returns true if all the nodes in the graph have a unique nodeID. */
    static inline bool areNodeIDsUnique (Node& node, bool ignoreZeroIDs)
    {
        std::vector<size_t> nodeIDs;
        visitNodes (node, [&] (Node& n) { nodeIDs.push_back (n.getNodeProperties().nodeID); }, false);
        std::sort (nodeIDs.begin(), nodeIDs.end());
        
        if (ignoreZeroIDs)
            nodeIDs.erase (std::remove_if (nodeIDs.begin(), nodeIDs.end(),
                                           [] (auto nID) { return nID == 0; }),
                           nodeIDs.end());
        
        auto uniqueEnd = std::unique (nodeIDs.begin(), nodeIDs.end());
        return uniqueEnd == nodeIDs.end();
    }

    //==============================================================================
    template<typename AudioFormatType>
    std::unique_ptr<juce::TemporaryFile> getSinFile (double sampleRate, double durationInSeconds)
    {
        juce::AudioBuffer<float> buffer (1, static_cast<int> (sampleRate * durationInSeconds));
        juce::dsp::Oscillator<float> osc ([] (float in) { return std::sin (in); });
        osc.setFrequency (220.0, true);
        osc.prepare ({ double (sampleRate), uint32_t (buffer.getNumSamples()), 1 });

        float* samples = buffer.getWritePointer (0);
        int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
            samples[i] = osc.processSample (0.0);

        // Then write it to a temp file
        AudioFormatType format;
        auto f = std::make_unique<juce::TemporaryFile> (format.getFileExtensions()[0]);
        
        if (auto fileStream = f->getFile().createOutputStream())
        {
            const int numQualityOptions = format.getQualityOptions().size();
            const int qualityOptionIndex = numQualityOptions == 0 ? 0 : (numQualityOptions / 2);
            const int bitDepth = format.getPossibleBitDepths().contains (16) ? 16 : 32;
            
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (AudioFormatType().createWriterFor (fileStream.get(), sampleRate, 1, bitDepth, {}, qualityOptionIndex)))
            {
                fileStream.release();
                writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            }
        }

        return f;
    }

    //==============================================================================
    /** Compares two MidiMessageSequences and expects them to be equal. */
    static inline void expectMidiMessageSequence (juce::UnitTest& ut, const juce::MidiMessageSequence& seq1, const juce::MidiMessageSequence& seq2)
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
                ut.logMessage (juce::String ("Event at index 123 is:\n\tseq1: XXX\n\tseq2 is: YYY")
                                .replace ("123", juce::String (i)).replace ("XXX", desc1).replace ("YYY", desc2));
                sequencesTheSame = false;
            }
        }
        
        ut.expect (sequencesTheSame, "MIDI sequence contents not equal");
    }

    /** Compares a MidiBuffer and a MidiMessageSequence and expects them to be equal. */
    static inline void expectMidiBuffer (juce::UnitTest& ut, const juce::MidiBuffer& buffer, double sampleRate, const juce::MidiMessageSequence& seq)
    {
        expectMidiMessageSequence (ut, createMidiMessageSequence (buffer, sampleRate), seq);
    }

    /** Expects a specific magnitude and RMS from an AudioBuffer's channel. */
    static inline void expectAudioBuffer (juce::UnitTest& ut, const juce::AudioBuffer<float>& buffer, int channel, float mag, float rms)
    {
        ut.expectWithinAbsoluteError (buffer.getMagnitude (channel, 0, buffer.getNumSamples()), mag, 0.001f);
        ut.expectWithinAbsoluteError (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()), rms, 0.001f);
    }

    /** Splits a buffer in to two and expects a specific magnitude and RMS from each half AudioBuffer. */
    static inline void expectAudioBuffer (juce::UnitTest& ut, juce::AudioBuffer<float>& buffer, int channel, int numSampleToSplitAt,
                                          float mag1, float rms1, float mag2, float rms2)
    {
        {
            juce::AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                    0, numSampleToSplitAt);
            expectAudioBuffer (ut, trimmedBuffer, channel, mag1, rms1);
        }
        
        {
            juce::AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(),
                                                    buffer.getNumChannels(),
                                                    numSampleToSplitAt, buffer.getNumSamples() - numSampleToSplitAt);
            expectAudioBuffer (ut, trimmedBuffer, channel, mag2, rms2);
        }
    }

    /** Extracts a section of a buffer and expects a specific magnitude and RMS it. */
    template<typename IntType>
    static inline void expectAudioBuffer (juce::UnitTest& ut, juce::AudioBuffer<float>& buffer, int channel, juce::Range<IntType> sampleRange,
                                          float mag, float rms)
    {
        juce::AudioBuffer<float> trimmedBuffer (buffer.getArrayOfWritePointers(),
                                                buffer.getNumChannels(),
                                                (int) sampleRange.getStart(), (int) sampleRange.getLength());
        expectAudioBuffer (ut, trimmedBuffer, channel, mag, rms);
    }

    /** Checks that there are no duplicate nodeIDs in a Node. */
    static inline void expectUniqueNodeIDs (juce::UnitTest& ut, Node& node, bool ignoreZeroIDs)
    {
        auto areUnique = areNodeIDsUnique (node, ignoreZeroIDs);
        ut.expect (areUnique, "nodeIDs are not unique");
        
        if (! areUnique)
            visitNodes (node, [&] (Node& n) { ut.logMessage (juce::String (typeid (n).name()) + " - " + juce::String (n.getNodeProperties().nodeID)); }, false);
    }

    //==============================================================================
    //==============================================================================
    struct TestSetup
    {
        double sampleRate = 44100.0;
        int blockSize = 512;
        bool randomiseBlockSizes = false;
        juce::Random random;
    };
    
    /** Returns a set of TestSetups to be used for testing. */
    static inline std::vector<TestSetup> getTestSetups (juce::UnitTest& ut)
    {
        std::vector<TestSetup> setups;
        
       #if JUCE_DEBUG
        for (double sampleRate : { 44100.0, 96000.0 })
            for (int blockSize : { 64, 512 })
       #else
        for (double sampleRate : { 44100.0, 48000.0, 96000.0 })
            for (int blockSize : { 64, 256, 512, 1024 })
       #endif
                for (bool randomiseBlockSizes : { false, true })
                    setups.push_back ({ sampleRate, blockSize, randomiseBlockSizes, ut.getRandom() });

        return setups;
    }

    //==============================================================================
    struct TestContext
    {
        std::unique_ptr<juce::TemporaryFile> tempFile;
        juce::AudioBuffer<float> buffer;
        juce::MidiBuffer midi;
        int numProcessMisses = 0;
    };

    template<typename NodeProcessorType>
    struct TestProcess
    {
        TestProcess (std::unique_ptr<NodeProcessorType> playerToUse, TestSetup ts,
                     const int numChannelsToUse, const double durationInSeconds)
            : testSetup (ts), numChannels (numChannelsToUse)
        {
            context = std::make_shared<TestContext>();
            context->tempFile = std::make_unique<juce::TemporaryFile> (".wav");

            buffer.setSize  (numChannels, ts.blockSize);
            numSamplesToDo = juce::roundToInt (durationInSeconds * ts.sampleRate);

            writer = std::unique_ptr<juce::AudioFormatWriter> (juce::WavAudioFormat().createWriterFor (context->tempFile->getFile().createOutputStream().release(),
                                                                                                       ts.sampleRate, (uint32_t) numChannels, 16, {}, 0));
            setPlayer (std::move (playerToUse));
        }

        void setNode (std::unique_ptr<Node> newNode)
        {
            jassert (newNode != nullptr);
            processor->setNode (std::move (newNode));
        }
        
        void setPlayer (std::unique_ptr<NodeProcessorType> newPlayerToUse)
        {
            jassert (newPlayerToUse != nullptr);
            processor = std::move (newPlayerToUse);
            processor->prepareToPlay (testSetup.sampleRate, testSetup.blockSize);
        }
                
        /** Processes a number of samples.
            Returns true if there is still more processing to be done.
        */
        bool process (int maxNumSamples)
        {
            if (! writer)
                return false;
            
            for (;;)
            {
                const int maxNumThisTime = testSetup.randomiseBlockSizes ? std::min (testSetup.random.nextInt ({ 1, testSetup.blockSize }), numSamplesToDo)
                                                                         : std::min (testSetup.blockSize, numSamplesToDo);
                const int numThisTime = std::min (maxNumSamples, maxNumThisTime);
                buffer.clear();
                midi.clear();
                
                juce::AudioBuffer<float> subSectionBuffer (buffer.getArrayOfWritePointers(), buffer.getNumChannels(),
                                                           0, numThisTime);

                numProcessMisses += processor->process ({ juce::Range<int64_t>::withStartAndLength ((int64_t) numSamplesDone, (int64_t) numThisTime),
                                                        { { subSectionBuffer }, midi } });
                
                writer->writeFromAudioSampleBuffer (subSectionBuffer, 0, subSectionBuffer.getNumSamples());
                
                // Copy MIDI to buffer
                for (const auto& m : midi)
                {
                    const int sampleNumber = (int) std::floor (m.getTimeStamp() * testSetup.sampleRate);
                    context->midi.addEvent (m, numSamplesDone + sampleNumber);
                }
                
                numSamplesToDo -= numThisTime;
                numSamplesDone += numThisTime;
                maxNumSamples -=  numThisTime;
                
                if (maxNumSamples <= 0)
                    break;
            }
            
            return numSamplesToDo > 0;
        }
        
        std::shared_ptr<TestContext> processAll()
        {
            process (numSamplesToDo);
            return getTestResult();
        }
        
        std::shared_ptr<TestContext> getTestResult()
        {
            writer->flush();

            // Then read it back in to the buffer
            if (auto reader = std::unique_ptr<juce::AudioFormatReader> (juce::WavAudioFormat().createReaderFor (context->tempFile->getFile().createInputStream().release(), true)))
            {
                juce::AudioBuffer<float> tempBuffer (numChannels, (int) reader->lengthInSamples);
                reader->read (&tempBuffer, 0, tempBuffer.getNumSamples(), 0, true, true);
                context->buffer = std::move (tempBuffer);
                context->numProcessMisses = numProcessMisses;
                
                return context;
            }
            
            return {};
        }
        
    private:
        std::unique_ptr<NodeProcessorType> processor;
        TestSetup testSetup;
        const int numChannels;

		std::shared_ptr<TestContext> context;
		std::unique_ptr<juce::AudioFormatWriter> writer;
        
        juce::AudioBuffer<float> buffer;
        tracktion_engine::MidiMessageArray midi;
        int numSamplesToDo = 0;
        int numSamplesDone = 0;
        int numProcessMisses = 0;
    };

    template<typename NodeProcessorType>
    static inline std::shared_ptr<TestContext> createTestContext (std::unique_ptr<NodeProcessorType> processor, TestSetup ts,
                                                                  const int numChannels, const double durationInSeconds)
    {
        return TestProcess<NodeProcessorType> (std::move (processor), ts, numChannels, durationInSeconds).processAll();
    }

    static inline std::shared_ptr<TestContext> createBasicTestContext (std::unique_ptr<Node> node, const TestSetup ts,
                                                                       const int numChannels, const double durationInSeconds)
    {
        auto player = std::make_unique<NodePlayer> (std::move (node));
        return createTestContext (std::move (player), ts, numChannels, durationInSeconds);
    }

    static inline std::shared_ptr<TestContext> createBasicTestContext (std::unique_ptr<Node> node, PlayHeadState& playHeadStateToUse,
                                                                       const TestSetup ts, const int numChannels, const double durationInSeconds)
    {
        auto player = std::make_unique<NodePlayer> (std::move (node), &playHeadStateToUse);
        return createTestContext (std::move (player), ts, numChannels, durationInSeconds);
    }
}

}
