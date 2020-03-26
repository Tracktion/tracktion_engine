/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

#if TRACKTION_UNIT_TESTS

//==============================================================================
//==============================================================================
class PDCTests  : public UnitTest
{
public:
    PDCTests()
        : UnitTest ("PDC", "Tracktion:Longer")
    {
    }

    void runTest() override
    {
        auto sinFile = getSinFile();

        auto& engine = *Engine::getEngines()[0];
        engine.getPluginManager().createBuiltInType<LatencyPlugin>();
        
        auto edit = createTestEdit (engine);
        auto track1 = getAudioTracks (*edit)[0];
        auto track2 = getAudioTracks (*edit)[1];
        expect (track1 != track2);

        beginTest ("No latency");
        {
            // Add sin file
            {
                AudioFile af (engine, sinFile->getFile());
                expect (af.isValid());
                expect (af.getLength() == 1.0);

                for (auto t : { track1, track2 })
                {
                    auto clip = t->insertWaveClip ("sin", af.getFile(), {{ 0.0, af.getLength() }}, false);
                    expectEquals (clip->getPosition().getStart(), 0.0);
                    expectEquals (clip->getPosition().getEnd(), 1.0);
                }
            }

            expectPeak (*edit, { 0.0, 1.0 }, { track1 }, 1.0f);

            // Set end to 0.5s for volume off
            track1->getClips()[0]->setPosition ({{ 0.0, 0.5 }});
            expectPeak (*edit, { 0.0, 0.5 }, { track1 }, 1.0f);
            expectPeak (*edit, { 0.5, 1.0 }, { track1 }, 0.0f);
        }

		engine.getAudioFileManager().releaseAllFiles();
        edit->getTempDirectory (false).deleteRecursively();
    }

    void expectPeak (Edit& edit, EditTimeRange tr, Array<Track*> tracks, float expectedPeak)
    {
        auto blockSize = edit.engine.getDeviceManager().getBlockSize();
        auto stats = logStats (Renderer::measureStatistics ("PDC Tests", edit, tr, getTracksMask (tracks), blockSize));
        expect (juce::isWithin (stats.peak, expectedPeak, 0.0001f), String ("Expected peak: ") + String (expectedPeak, 4));
    }

    Renderer::Statistics logStats (Renderer::Statistics stats)
    {
        logMessage ("Stats: peak " + String (stats.peak) + ", avg " + String (stats.average) + ", duration " + String (stats.audioDuration));
        return stats;
    }

    static BigInteger getTracksMask (const Array<Track*>& tracks)
    {
        BigInteger tracksMask;

        for (auto t : tracks)
            tracksMask.setBit (t->getIndexInEditTrackList());

        jassert (tracksMask.countNumberOfSetBits() == tracks.size());
        return tracksMask;
    }

    //==============================================================================
    static std::unique_ptr<Edit> createTestEdit (Engine& engine)
    {
        // Make tempo 60bpm and 0dB master vol for easy calculations
        auto edit = Edit::createSingleTrackEdit (engine);
        edit->tempoSequence.getTempo (0)->setBpm (60.0);
        edit->getMasterVolumePlugin()->setVolumeDb (0.0);

        // Create two AudioTracks
        edit->ensureNumberOfAudioTracks (2);

        return edit;
    }

    static std::unique_ptr<TemporaryFile> getSinFile()
    {
        // Create a 1s sin buffer
        auto sampleRate = 44100;
        AudioBuffer<float> buffer (1, sampleRate);
        juce::dsp::Oscillator<float> osc ([] (float in) { return std::sin (in); });
        osc.setFrequency (220.0);
        osc.prepare ({ double (sampleRate), uint32 (sampleRate), 1 });

        float* samples = buffer.getWritePointer (0);
        int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
            samples[i] = osc.processSample (0.0);

        // Then write it to a temp file
        auto f = std::make_unique<TemporaryFile> (".wav");
        
        if (auto fileStream = f->getFile().createOutputStream())
        {
            if (auto writer = std::unique_ptr<AudioFormatWriter> (WavAudioFormat().createWriterFor (fileStream.get(), (double) 44100, 1, 16, {}, 0)))
            {
                fileStream.release();
                writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            }
        }

        return f;
    }
};

static PDCTests pdcTests;

#endif

} // namespace tracktion_engine
