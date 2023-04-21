/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include <numeric>
#include <juce_audio_formats/juce_audio_formats.h>

namespace tracktion { inline namespace graph
{

//==============================================================================
namespace test_utilities
{
    /** Creates a random MidiMessageSequence sequence. */
    static inline juce::MidiMessageSequence createRandomMidiMessageSequence (double durationSeconds, juce::Random r,
                                                                             juce::Range<double> noteLengthRange = { 0.1, 0.6 })
    {
        juce::MidiMessageSequence sequence;
        int noteNumber = -1;

        for (double time = 0.0; time < durationSeconds;
             time += r.nextDouble() * noteLengthRange.getLength() + noteLengthRange.getStart())
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

        sequence.updateMatchedPairs();

        return sequence;
    }

    /** Creates a MidiMessageSequence from a MidiBuffer. */
    static inline juce::MidiMessageSequence createMidiMessageSequence (const juce::MidiBuffer& buffer, double sampleRate)
    {
        juce::MidiMessageSequence sequence;

        for (auto itr : buffer)
        {
            const auto& result = itr.getMessage();
            int samplePosition = itr.samplePosition;

            const double time = samplePosition / sampleRate;
            sequence.addEvent (result, time);
        }

        return sequence;
    }

    //==============================================================================
    static inline float getPhaseIncrement (float frequency, double sampleRate)
    {
        return (float) ((frequency * juce::MathConstants<double>::pi * 2.0) / sampleRate);
    }

    struct SineOscillator
    {
        void reset (float frequency, double sampleRate)
        {
            phase = 0;
            phaseIncrement = getPhaseIncrement (frequency, sampleRate);
        }

        float getNext()
        {
            auto v = std::sin (phase);
            phase = std::fmod (phase + phaseIncrement, juce::MathConstants<float>::pi * 2.0f);
            return v;
        }

        float phase = 0, phaseIncrement = 0;
    };

    static inline void fillBufferWithSinData (choc::buffer::ChannelArrayView<float> buffer)
    {
        auto phaseIncrement = juce::MathConstants<float>::twoPi / buffer.getNumFrames();
        setAllFrames (buffer, [=] (auto frame) { return std::sin ((float) (frame * phaseIncrement)); });
    }

    static inline auto createSineBuffer (int numChannels, int numFrames, double phaseIncrement)
    {
        return choc::buffer::createChannelArrayBuffer (numChannels, numFrames,
                                                       [=] (auto, auto frame) { return std::sin ((float) (frame * phaseIncrement)); });
    }

    /** Logs a MidiMessageSequence. */
    static inline void logMidiMessageSequence (juce::UnitTest& ut, const juce::MidiMessageSequence& seq)
    {
        for (int i = 0; i < seq.getNumEvents(); ++i)
            ut.logMessage (seq.getEventPointer (i)->message.getDescription() + " - " + juce::String (seq.getEventPointer (i)->message.getTimeStamp()));
    }

    /** Logs a MidiBuffer. */
    static inline void dgbMidiBuffer (const juce::MidiBuffer& buffer)
    {
        ignoreUnused (buffer);
       #if JUCE_DEBUG
        for (auto itr : buffer)
        {
            const auto& result = itr.getMessage();
            DBG(result.getDescription());
        }
       #endif
    }

    /** Removes any non-note event messages from a sequence. */
    static inline juce::MidiMessageSequence stripNonNoteOnOffMessages (juce::MidiMessageSequence seq)
    {
        for (int i = seq.getNumEvents(); --i >= 0;)
            if (! seq.getEventPointer (i)->message.isNoteOnOrOff())
                seq.deleteEvent (i, false);

        return seq;
    }

    /** Removes any meta-event messages from a sequence. */
    static inline juce::MidiMessageSequence stripMetaEvents (juce::MidiMessageSequence seq)
    {
        for (int i = seq.getNumEvents(); --i >= 0;)
            if (! seq.getEventPointer (i)->message.isMetaEvent())
                seq.deleteEvent (i, false);

        return seq;
    }

    /** Writes an audio buffer to a file. */
    static inline void writeToFile (juce::File file, choc::buffer::ChannelArrayView<float> block, double sampleRate)
    {
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (juce::WavAudioFormat().createWriterFor (file.createOutputStream().release(),
                                                                                                            sampleRate,
                                                                                                            block.getNumChannels(),
                                                                                                            16, {}, 0)))
        {
            writer->writeFromAudioSampleBuffer (toAudioBuffer (block), 0, (int) block.getNumFrames());
        }
    }

    //==============================================================================
    /** Returns the ammount of internal memory allocated for buffers. */
    static inline size_t getMemoryUsage (const std::vector<Node*>& nodes)
    {
        return std::accumulate (nodes.begin(), nodes.end(), (size_t) 0,
                                [] (size_t total, Node* n)
                                {
                                    return total + n->getAllocatedBytes();
                                });
    }

    /** Returns the ammount of internal memory allocated for buffers. */
    static inline size_t getMemoryUsage (Node& node)
    {
        return getMemoryUsage (tracktion::graph::getNodes (node, tracktion::graph::VertexOrdering::postordering));
    }

    /** Returns the ammount of internal memory allocated for buffers. */
    inline juce::String getName (ThreadPoolStrategy type)
    {
        switch (type)
        {
            case ThreadPoolStrategy::conditionVariable:     return "conditionVariable";
            case ThreadPoolStrategy::realTime:              return "realTime";
            case ThreadPoolStrategy::hybrid:                return "hybrid";
            case ThreadPoolStrategy::semaphore:             return "semaphore";
            case ThreadPoolStrategy::lightweightSemaphore:  return "lightweightSemaphore";
            case ThreadPoolStrategy::lightweightSemHybrid:  return "lightweightSemaphoreHybrid";
        }

        jassertfalse;
        return {};
    }

    inline std::vector<ThreadPoolStrategy> getThreadPoolStrategies()
    {
        return { ThreadPoolStrategy::lightweightSemHybrid,
                 ThreadPoolStrategy::lightweightSemaphore,
                 ThreadPoolStrategy::semaphore,
                 ThreadPoolStrategy::conditionVariable,
                 ThreadPoolStrategy::realTime,
                 ThreadPoolStrategy::hybrid };
    }

    /** Logs the graph structure to the console. */
    inline void logGraph (Node& node)
    {
        struct Visitor
        {
            static void logNode (Node& n, int depth)
            {
                juce::Logger::writeToLog (juce::String::repeatedString (" ", depth * 2) + typeid (n).name());
            }

            static void visitInputs (Node& n, int depth)
            {
                logNode (n, depth);
                
                for (auto input : n.getDirectInputNodes())
                    visitInputs (*input, depth + 1);
            }
        };
        
        Visitor::visitInputs (node, 0);
    }

    /** Returns the graph structure in a dot textual description.
        Save this to a file and then run it through graphviz's dot program to plot the graph.
        E.g. `cat output.txt | dot -Tsvg > output.svg`
    */
    std::string createGraphDescription (Node&);


    //==============================================================================
    template<typename AudioFormatType>
    std::unique_ptr<juce::TemporaryFile> getSinFile (double sampleRate, double durationInSeconds,
                                                     int numChannels = 1, float frequency = 220.0f)
    {
        auto buffer = createSineBuffer (numChannels, (int) (sampleRate * durationInSeconds),
                                        getPhaseIncrement (frequency, sampleRate));

        AudioFormatType format;
        auto f = std::make_unique<juce::TemporaryFile> (format.getFileExtensions()[0]);
        writeToFile (f->getFile(), buffer, sampleRate);
        return f;
    }

    //==============================================================================
    /** Compares two MidiMessageSequences and expects them to be equal. */
    static inline void expectMidiMessageSequence (juce::UnitTest& ut, const juce::MidiMessageSequence& actual, const juce::MidiMessageSequence& expected)
    {
        ut.expectEquals (actual.getNumEvents(), expected.getNumEvents(), "Num MIDI events not equal");

        bool sequencesTheSame = true;

        for (int i = 0; i < std::min (actual.getNumEvents(), expected.getNumEvents()); ++i)
        {
            auto event1 = actual.getEventPointer (i);
            auto event2 = expected.getEventPointer (i);

            auto desc1 = event1->message.getDescription();
            auto desc2 = event2->message.getDescription();

            if (desc1 != desc2)
            {
                ut.logMessage (juce::String ("Event at index 123 is:\n\tactual: XXX\n\texpected is: YYY")
                                .replace ("123", juce::String (i)).replace ("XXX", desc1).replace ("YYY", desc2));
                sequencesTheSame = false;
            }
        }

        for (int i = actual.getNumEvents(); i < expected.getNumEvents(); ++i)
        {
            auto event2 = expected.getEventPointer (i);
            auto desc2 = event2->message.getDescription();
            ut.logMessage (juce::String ("Missing event at index 123 is:\n\texpected is: YYY, TTT")
                            .replace ("123", juce::String (i)).replace ("YYY", desc2).replace ("TTT", juce::String (event2->message.getTimeStamp())));
        }

        for (int i = expected.getNumEvents(); i < actual.getNumEvents(); ++i)
        {
            auto event2 = actual.getEventPointer (i);
            auto desc2 = event2->message.getDescription();
            ut.logMessage (juce::String ("Extra event at index 123 is:\n\tactual is: YYY, TTT")
                            .replace ("123", juce::String (i)).replace ("YYY", desc2).replace ("TTT", juce::String (event2->message.getTimeStamp())));
        }

        ut.expect (sequencesTheSame, "MIDI sequence contents not equal");

        if (! sequencesTheSame)
        {
            ut.logMessage ("Actual:");
            logMidiMessageSequence (ut, actual);
            ut.logMessage ("Expected:");
            logMidiMessageSequence (ut, expected);
        }
    }

    /** Compares a MidiBuffer and a MidiMessageSequence and expects them to be equal. */
    static inline void expectMidiBuffer (juce::UnitTest& ut, const juce::MidiBuffer& buffer, double sampleRate, const juce::MidiMessageSequence& seq)
    {
        expectMidiMessageSequence (ut, createMidiMessageSequence (buffer, sampleRate), seq);
    }

    /** Expects a specific magnitude and RMS from an AudioBuffer's channel. */
    static inline void expectAudioBuffer (juce::UnitTest& ut, const juce::AudioBuffer<float>& buffer, int channel, float mag, float rms)
    {
        ut.expectWithinAbsoluteError (buffer.getMagnitude (channel, 0, buffer.getNumSamples()), mag, 0.01f);
        ut.expectWithinAbsoluteError (buffer.getRMSLevel (channel, 0, buffer.getNumSamples()), rms, 0.01f);
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
        auto areUnique = node_player_utils::areNodeIDsUnique (node, ignoreZeroIDs);
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

    /** Returns a the descriptions of s TestSetup. */
    static inline juce::String getDescription (const TestSetup& ts)
    {
        return juce::String (ts.sampleRate, 0) + ", " + juce::String (ts.blockSize)
            + juce::String (ts.randomiseBlockSizes ? ", random" : "");
    }

    /** Returns a set of TestSetups to be used for testing. */
    static inline std::vector<TestSetup> getTestSetups (juce::UnitTest& ut)
    {
        std::vector<TestSetup> setups;

       #if JUCE_DEBUG || GRAPH_UNIT_TESTS_QUICK_VALIDATE
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
        juce::AudioBuffer<float> buffer;
        juce::MidiBuffer midi;
        int numProcessMisses = 0;
    };

    template<typename NodePlayerType>
    struct TestProcess
    {
        TestProcess (std::unique_ptr<NodePlayerType> playerToUse, TestSetup ts,
                     const int numChannelsToUse, const double duration,
                     bool writeToBuffer)
            : testSetup (ts), numChannels (numChannelsToUse), durationInSeconds (duration)
        {
            context = std::make_shared<TestContext>();

            buffer.resize  ({ (choc::buffer::ChannelCount) numChannels, (choc::buffer::FrameCount) ts.blockSize });
            numSamplesToDo = juce::roundToInt (durationInSeconds * ts.sampleRate);

            if (writeToBuffer && numChannels > 0)
            {
                if (auto os = std::make_unique<juce::MemoryOutputStream> (audioOutputBlock, false))
                {
                    writer = std::unique_ptr<juce::AudioFormatWriter> (juce::WavAudioFormat().createWriterFor (os.release(),
                                                                                                               ts.sampleRate, (uint32_t) numChannels, 16, {}, 0));
                }
                else
                {
                    jassertfalse;
                }
            }
            
            setPlayer (std::move (playerToUse));
        }

        /** Returns a description of the number of channels and length of rendering. */
        std::string getDescription() const
        {
            return juce::String ("{numChannels} channels, {durationInSeconds}s")
                    .replace ("{numChannels}", juce::String (numChannels))
                    .replace ("{durationInSeconds}", juce::String (durationInSeconds)).toStdString();
        }
        
        PerformanceMeasurement::Statistics getStatisticsAndReset()
        {
            return performanceMeasurement.getStatisticsAndReset();
        }
        
        Node& getNode() const
        {
            return *player->getNode();
        }

        NodePlayerType& getNodePlayer() const
        {
            return *player;
        }

        void setNode (std::unique_ptr<Node> newNode)
        {
            jassert (newNode != nullptr);
            player->setNode (std::move (newNode));
        }

        void setPlayer (std::unique_ptr<NodePlayerType> newPlayerToUse)
        {
            jassert (newPlayerToUse != nullptr);
            player = std::move (newPlayerToUse);
            player->prepareToPlay (testSetup.sampleRate, testSetup.blockSize);
        }

        void setPlayHead (tracktion::graph::PlayHead* newPlayHead)
        {
            playHead = newPlayHead;
        }

        /** Processes a number of samples.
            Returns true if there is still more processing to be done.
        */
        bool process (int maxNumSamples)
        {
            for (;;)
            {
                const ScopedPerformanceMeasurement spm (performanceMeasurement);
                
                auto maxNumThisTime = testSetup.randomiseBlockSizes ? std::min (testSetup.random.nextInt ({ 1, testSetup.blockSize }), numSamplesToDo)
                                                                    : std::min (testSetup.blockSize, numSamplesToDo);
                auto numThisTime = std::min (maxNumSamples, maxNumThisTime);
                midi.clear();

                auto subSectionView = buffer.getStart ((choc::buffer::FrameCount) numThisTime);
                subSectionView.clear();

                const auto referenceSampleRange = juce::Range<int64_t>::withStartAndLength ((int64_t) numSamplesDone, (int64_t) numThisTime);

                if (playHead)
                    playHead->setReferenceSampleRange (referenceSampleRange);

                numProcessMisses += player->process ({ (choc::buffer::FrameCount) numThisTime, referenceSampleRange, { subSectionView, midi } });

                if (writer)
                {
                    auto audioBuffer = tracktion::graph::toAudioBuffer (subSectionView);
                    writer->writeFromAudioSampleBuffer (audioBuffer, 0, audioBuffer.getNumSamples());
                }

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
            if (writer)
            {
                writer->flush();

                // Then read it back in to the buffer
                if (auto is = std::make_unique<juce::MemoryInputStream> (audioOutputBlock, false))
                {
                    if (auto reader = std::unique_ptr<juce::AudioFormatReader> (juce::WavAudioFormat().createReaderFor (is.release(), true)))
                    {
                        juce::AudioBuffer<float> tempBuffer (numChannels, (int) reader->lengthInSamples);
                        reader->read (&tempBuffer, 0, tempBuffer.getNumSamples(), 0, true, true);
                        context->buffer = std::move (tempBuffer);
                    }
                }
            }

            return context;
        }

    private:
        std::unique_ptr<NodePlayerType> player;
        TestSetup testSetup;
        const int numChannels;
        const double durationInSeconds;
        tracktion::graph::PlayHead* playHead = nullptr;

        std::shared_ptr<TestContext> context;
        juce::MemoryBlock audioOutputBlock;
        std::unique_ptr<juce::AudioFormatWriter> writer;

        choc::buffer::ChannelArrayBuffer<float> buffer;
        tracktion_engine::MidiMessageArray midi;
        int numSamplesToDo = 0;
        int numSamplesDone = 0;
        int numProcessMisses = 0;
        
        PerformanceMeasurement performanceMeasurement { "TestProcess" , -1 };
    };

    template<typename NodePlayerType>
    static inline std::shared_ptr<TestContext> createTestContext (std::unique_ptr<NodePlayerType> player, TestSetup ts,
                                                                  const int numChannels, const double durationInSeconds)
    {
        return TestProcess<NodePlayerType> (std::move (player), ts, numChannels, durationInSeconds, true).processAll();
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

}}
