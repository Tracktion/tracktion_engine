/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{

#ifndef DOXYGEN
namespace test_utilities
{
    //==============================================================================
    inline Renderer::Statistics logStats (juce::UnitTest& ut, Renderer::Statistics stats)
    {
        ut.logMessage ("Stats: peak " + juce::String (stats.peak) + ", avg " + juce::String (stats.average)
                       + ", duration " + juce::String (stats.audioDuration));
        return stats;
    }

    inline void expectPeak (juce::UnitTest& ut, Edit& edit, TimeRange tr, juce::Array<Track*> tracks, float expectedPeak)
    {
        auto blockSize = edit.engine.getDeviceManager().getBlockSize();
        auto stats = logStats (ut, Renderer::measureStatistics ("Tests", edit, tr, toBitSet (tracks), blockSize));
        ut.expect (juce::isWithin (stats.peak, expectedPeak, 0.001f), juce::String ("Expected peak: ") + juce::String (expectedPeak, 4));
    }

    //==============================================================================
    inline std::unique_ptr<juce::TemporaryFile> renderToTempFileAndLogPath (Edit& edit)
    {
        auto tf = std::make_unique<juce::TemporaryFile> (".wav");
        Renderer::renderToFile (edit, tf->getFile(), false);
        DBG(tf->getFile().getFullPathName());
        return tf;
    }

    struct BufferAndSampleRate
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 0;
        std::unique_ptr<juce::TemporaryFile> file;
    };

    inline BufferAndSampleRate renderToAudioBuffer (Edit& edit)
    {
        auto tf = std::make_unique<juce::TemporaryFile> (".wav");
        Renderer::renderToFile (edit, tf->getFile(), false);

        juce::AudioFormatManager manager;
        manager.registerFormat (new juce::WavAudioFormat(), true);

        if (auto reader = std::unique_ptr<juce::AudioFormatReader> (manager.createReaderFor (tf->getFile())))
        {
            juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels),
                                             static_cast<int> (reader->lengthInSamples));

            if (reader->read (&buffer, 0, buffer.getNumSamples(),
                              0, true, buffer.getNumChannels() > 1))
            {
                return { buffer, reader->sampleRate, std::move (tf) };
            }
        }

        return {};
    }

    inline void expectPeak (juce::UnitTest& ut, const BufferAndSampleRate& data, TimeRange tr, float expectedPeak)
    {
        const auto sampleRange = toSamples (tr, data.sampleRate);
        const auto peak = data.buffer.getMagnitude (static_cast<int> (sampleRange.getStart()),
                                                    static_cast<int> (sampleRange.getLength()));
        ut.expect (juce::isWithin (peak, expectedPeak, 0.001f),
                   juce::String ("Expected peak: ") + juce::String (expectedPeak, 4) + ", actual peak: " + juce::String (peak, 4));
    }

    inline void expectRMS (juce::UnitTest& ut, const BufferAndSampleRate& data, TimeRange tr, int channel, float expectedRMS)
    {
        const auto sampleRange = toSamples (tr, data.sampleRate);
        ut.expectWithinAbsoluteError (data.buffer.getRMSLevel (channel,
                                                               static_cast<int> (sampleRange.getStart()),
                                                               static_cast<int> (sampleRange.getLength())),
                                      expectedRMS, 0.01f);
    }

    //==============================================================================
    inline std::unique_ptr<Edit> createTestEdit (Engine& engine, int numAudioTracks = 1)
    {
        // Make tempo 60bpm and 0dB master vol for easy calculations
        auto edit = std::make_unique<Edit> (engine, createEmptyEdit (engine), Edit::forRendering, nullptr, 1);
        edit->ensureNumberOfAudioTracks (numAudioTracks);

        edit->tempoSequence.getTempo (0)->setBpm (60.0);
        edit->getMasterVolumePlugin()->setVolumeDb (0.0);

        return edit;
    }
}
#endif

}} // namespace tracktion { inline namespace engine
