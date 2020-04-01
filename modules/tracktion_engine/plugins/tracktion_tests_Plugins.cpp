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
        auto sinFile = getSinFile<WavAudioFormat> (44100.0);

        auto& engine = *Engine::getEngines()[0];
        engine.getPluginManager().createBuiltInType<LatencyPlugin>();
        
        beginTest ("No latency");
        {
            expectEquals (sinFile->getFile().getFileExtension(), String (".wav"));
            auto edit = createTestEdit (engine);
            auto track1 = getAudioTracks (*edit)[0];
            auto track2 = getAudioTracks (*edit)[1];
            expect (track1 != track2);
            
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

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }
        
        beginTest ("Source file with different sample rate");
        {
            auto sinFile96Ogg = getSinFile<OggVorbisAudioFormat> (96000.0);
            expectEquals (sinFile96Ogg->getFile().getFileExtension(), String (".ogg"));
            auto edit = createTestEdit (engine);
            auto track1 = getAudioTracks (*edit)[0];

            // Add sin file
            {
                AudioFile af (engine, sinFile96Ogg->getFile());
                expect (af.isValid());
                expect (af.getLength() == 1.0);

                for (auto t : { track1 })
                {
                    auto clip = t->insertWaveClip ("sin", af.getFile(), {{ 0.0, af.getLength() }}, false);
                    expectEquals (clip->getPosition().getStart(), 0.0);
                    expectEquals (clip->getPosition().getEnd(), 1.0);
                }
            }

            expectPeak (*edit, { 0.0, 1.0 }, { track1 }, 1.0f);

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }
    }

    void expectPeak (Edit& edit, EditTimeRange tr, Array<Track*> tracks, float expectedPeak)
    {
        auto blockSize = edit.engine.getDeviceManager().getBlockSize();
        auto stats = logStats (Renderer::measureStatistics ("PDC Tests", edit, tr, getTracksMask (tracks), blockSize));
        expect (juce::isWithin (stats.peak, expectedPeak, 0.001f), String ("Expected peak: ") + String (expectedPeak, 4));
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

    template<typename AudioFormatType>
    std::unique_ptr<TemporaryFile> getSinFile (double sampleRate)
    {
        // Create a 1s sin buffer
        AudioBuffer<float> buffer (1, (int) sampleRate);
        juce::dsp::Oscillator<float> osc ([] (float in) { return std::sin (in); });
        osc.setFrequency (220.0);
        osc.prepare ({ double (sampleRate), uint32 (sampleRate), 1 });

        float* samples = buffer.getWritePointer (0);
        int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
            samples[i] = osc.processSample (0.0);

        // Then write it to a temp file
        AudioFormatType format;
        auto f = std::make_unique<TemporaryFile> (format.getFileExtensions()[0]);
        
        if (auto fileStream = f->getFile().createOutputStream())
        {
            const int numQualityOptions = format.getQualityOptions().size();
            const int qualityOptionIndex = numQualityOptions == 0 ? 0 : (numQualityOptions / 2);
            const int bitDepth = format.getPossibleBitDepths().contains (16) ? 16 : 32;
            
            if (auto writer = std::unique_ptr<AudioFormatWriter> (AudioFormatType().createWriterFor (fileStream.get(), sampleRate, 1, bitDepth, {}, qualityOptionIndex)))
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
