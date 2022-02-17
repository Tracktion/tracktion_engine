/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

#if TRACKTION_UNIT_TESTS

//==============================================================================
//==============================================================================
class PDCTests  : public juce::UnitTest
{
public:
    PDCTests()
        : juce::UnitTest ("PDC", "Tracktion:Longer")
    {
    }

    void runTest() override
    {
        auto sinFile = getSinFile<juce::WavAudioFormat> (44100.0);

        auto& engine = *Engine::getEngines()[0];
        engine.getPluginManager().createBuiltInType<LatencyPlugin>();
        
        beginTest ("No latency");
        {
            expectEquals (sinFile->getFile().getFileExtension(), juce::String (".wav"));
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
                    auto clip = t->insertWaveClip ("sin", af.getFile(), {{ TimePosition(), TimePosition::fromSeconds (af.getLength()) }}, false);
                    expectEquals (clip->getPosition().getStart(), TimePosition());
                    expectEquals (clip->getPosition().getEnd(), TimePosition::fromSeconds (1.0));
                }
            }

            expectPeak (*edit, { 0.0s, TimePosition (1.0s) }, { track1, track2 }, 2.0f);

            // Set end to 0.5s for volume off
            track1->getClips()[0]->setPosition ({{ TimePosition(), TimePosition::fromSeconds (0.5) }});
            expectPeak (*edit, { 0.0s, TimePosition (0.5s) }, { track1, track2 }, 2.0f);
            expectPeak (*edit, { 0.5s, TimePosition (1.0s) }, { track1, track2 }, 1.0f);

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }
        
        beginTest ("Source file with different sample rate");
        {
            auto sinFile96Ogg = getSinFile<juce::OggVorbisAudioFormat> (96000.0);
            expectEquals (sinFile96Ogg->getFile().getFileExtension(), juce::String (".ogg"));
            auto edit = createTestEdit (engine);
            auto track1 = getAudioTracks (*edit)[0];

            // Add sin file
            {
                AudioFile af (engine, sinFile96Ogg->getFile());
                expect (af.isValid());
                expect (af.getLength() == 1.0);

                for (auto t : { track1 })
                {
                    auto clip = t->insertWaveClip ("sin", af.getFile(), {{ TimePosition(), TimePosition::fromSeconds (af.getLength()) }}, false);
                    expectEquals (clip->getPosition().getStart(), TimePosition());
                    expectEquals (clip->getPosition().getEnd(), TimePosition::fromSeconds (1.0));
                }
            }

            expectPeak (*edit, { 0.0s, TimePosition (1.0s) }, { track1 }, 1.0f);

            engine.getAudioFileManager().releaseAllFiles();
            edit->getTempDirectory (false).deleteRecursively();
        }
    }

    void expectPeak (Edit& edit, TimeRange tr, juce::Array<Track*> tracks, float expectedPeak)
    {
        auto blockSize = edit.engine.getDeviceManager().getBlockSize();
        auto stats = logStats (Renderer::measureStatistics ("PDC Tests", edit, tr, getTracksMask (tracks), blockSize));
        expect (juce::isWithin (stats.peak, expectedPeak, 0.001f), juce::String ("Expected peak: ") + juce::String (expectedPeak, 4));
    }

    Renderer::Statistics logStats (Renderer::Statistics stats)
    {
        logMessage ("Stats: peak " + juce::String (stats.peak) + ", avg " + juce::String (stats.average)
                     + ", duration " + juce::String (stats.audioDuration));
        return stats;
    }

    static juce::BigInteger getTracksMask (const juce::Array<Track*>& tracks)
    {
        juce::BigInteger tracksMask;

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
    std::unique_ptr<juce::TemporaryFile> getSinFile (double sampleRate)
    {
        // Create a 1s sin buffer
        juce::AudioBuffer<float> buffer (1, (int) sampleRate);
        juce::dsp::Oscillator<float> osc ([] (float in) { return std::sin (in); });
        osc.setFrequency (220.0);
        osc.prepare ({ double (sampleRate), uint32_t (sampleRate), 1 });

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
            
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (AudioFormatType().createWriterFor (fileStream.get(),
                                                                                                           sampleRate, 1, bitDepth,
                                                                                                           {}, qualityOptionIndex)))
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

}} // namespace tracktion { inline namespace engine
