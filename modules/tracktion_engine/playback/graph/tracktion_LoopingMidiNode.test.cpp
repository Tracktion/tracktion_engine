/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if ENGINE_UNIT_TESTS_LOOPINGMIDINODE

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class LoopingMidiNodeTests : public juce::UnitTest
{
public:
    LoopingMidiNodeTests()
        : juce::UnitTest ("LoopingMidiNodeTests", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        for (auto setup : tracktion::graph::test_utilities::getTestSetups (*this))
        {
            logMessage (juce::String ("Test setup: sample rate SR, block size BS, random blocks RND")
                        .replace ("SR", juce::String (setup.sampleRate))
                        .replace ("BS", juce::String (setup.blockSize))
                        .replace ("RND", setup.randomiseBlockSizes ? "Y" : "N"));

            runMidiTests (setup);
        }
    }

private:
    //==============================================================================
    //==============================================================================
    void runMidiTests (test_utilities::TestSetup ts)
    {
        beginTest ("Setup");

        // Create a random sequence, loop it, render it and check its the same when proxy is on/off
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine);

        auto r = ts.random;
        const auto duration = edit->tempoSequence.toTime ({ 4, 0_bd });
        const auto seq = test_utilities::createRandomMidiMessageSequence (duration.inSeconds(), r);

        // Add sequnce and loop randomly in the first and last of the 4 bars (16 beats)
        auto mc = getAudioTracks (*edit)[0]->insertMIDIClip ({ 0_tp, duration }, nullptr);
        expect (mc->canUseProxy());
        mc->getSequence().importMidiSequence (seq, nullptr, 0_tp, nullptr);
        mc->setNumberOfLoops (1);
        const auto loopStart    = 1.0 + r.nextDouble() * 3.0;
        const auto loopEnd      = 16.0 - (1.0 + r.nextDouble() * 3.0);
        mc->setLoopRangeBeats ({ BeatPosition::fromBeats (loopStart), BeatPosition::fromBeats (loopEnd) });

        // Loop for 30s
        mc->setEnd (30_tp, true);

        // Basic sequence
        beginTest ("Looped sequence");
        testMidiClip (*mc, ts);

        // Quantised sequence
        beginTest ("Quantised sequence");
        QuantisationType quantisation;
        quantisation.setType ("1/16");
        mc->setQuantisation (quantisation);
        testMidiClip (*mc, ts);

        // Grooved sequence
        beginTest ("Grooved sequence");
        auto& gtm = engine.getGrooveTemplateManager();
        expect (gtm.getNumTemplates() > 0);
        mc->setGrooveTemplate (gtm.getTemplateName (0));
        mc->setGrooveStrength (1.0f);
        testMidiClip (*mc, ts);
    }

    void testMidiClip (MidiClip& mc, test_utilities::TestSetup ts)
    {
        auto renderOpts = RenderOptions::forClipRender ({ &mc }, true);
        renderOpts->setFormat (RenderOptions::midi);
        renderOpts->setIncludePlugins (false);
        auto params = renderOpts->getRenderParameters (mc);
        params.sampleRateForAudio = ts.sampleRate;
        params.blockSizeForAudio = ts.blockSize;

        juce::TemporaryFile t1, t2;

        mc.setUsesProxy (true);
        expect (mc.canUseProxy());
        params.destFile = t1.getFile();
        const auto seqWithProxyFile = Renderer::renderToFile ("proxy", params);

        mc.setUsesProxy (false);
        expect (! mc.canUseProxy());
        params.destFile = t2.getFile();
        const auto seqWithoutProxyFile = Renderer::renderToFile ("non-proxy", params);

        test_utilities::expectMidiMessageSequence (*this, getSeqFromFile (seqWithProxyFile), getSeqFromFile (seqWithProxyFile));
    }

    static juce::MidiMessageSequence getSeqFromFile (juce::File f)
    {
        juce::MidiFile midiFile;

        {
            juce::FileInputStream in (f);

            if (! in.openedOk() || ! midiFile.readFrom (in))
                return {};
        }

        // Track 0 contains meta events from the tempo track etc.
        return *midiFile.getTrack (1);
    }
};

static LoopingMidiNodeTests loopingMidiNodeTests;

}} // namespace tracktion { inline namespace engine

#endif //ENGINE_UNIT_TESTS_LOOPINGMIDINODE
