/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
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
   #if JUCE_MODAL_LOOPS_PERMITTED
    inline bool runDispatchLoopUntilTrue (const std::atomic<bool>& flag)
    {
        for (;;)
        {
            if (flag)
                return false;

            if (! juce::MessageManager::getInstance()->runDispatchLoopUntil (1))
                return false;
        }

        return true;
    }
   #endif

    //==============================================================================
    inline std::optional<juce::AudioBuffer<float>> loadFileInToBuffer (juce::AudioFormatManager& formatManager, const juce::File& f)
    {
        if (auto reader = std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (f)))
        {
            juce::AudioBuffer<float> destBuffer (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));

            if (! reader->read (&destBuffer,
                                0, destBuffer.getNumSamples(), 0,
                                true, true))
                return {};

            return destBuffer;
        }

        return {};
    }

    inline std::optional<juce::AudioBuffer<float>> loadFileInToBuffer (Engine& e, const juce::File& f)
    {
        return loadFileInToBuffer (e.getAudioFileFormatManager().readFormatManager, f);
    }

    //==============================================================================
    void expectInt (juce::UnitTest& ut, std::unsigned_integral auto int1, std::unsigned_integral auto int2)
    {
        ut.expectEquals (static_cast<std::uint64_t> (int1), static_cast<std::uint64_t> (int2));
    }

    /* Only valid for values less than std::int64_t::max. */
    void expectInt (juce::UnitTest& ut, std::integral auto int1, std::integral auto int2)
    {
        assert (int1 <= std::numeric_limits<std::int64_t>::max());
        assert (int2 <= std::numeric_limits<std::int64_t>::max());
        ut.expectEquals (static_cast<std::int64_t> (int1), static_cast<std::int64_t> (int2));
    }

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
    struct TempCurrentWorkingDirectory
    {
        TempCurrentWorkingDirectory()
        {
            tempDir.createDirectory();
            tempDir.setAsCurrentWorkingDirectory();
        }

        ~TempCurrentWorkingDirectory()
        {
            tempDir.deleteRecursively (false);
            originalCwd.setAsCurrentWorkingDirectory();
        }

        juce::File originalCwd = juce::File::getCurrentWorkingDirectory();
        juce::File tempDir = juce::File::createTempFile ({});
    };

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
    inline std::unique_ptr<Edit> createTestEdit (Engine& engine, int numAudioTracks = 1, Edit::EditRole role = Edit::EditRole::forRendering)
    {
        // Make tempo 60bpm and 0dB master vol for easy calculations

        auto edit = Edit::createSingleTrackEdit (engine, role);

        edit->ensureNumberOfAudioTracks (numAudioTracks);
        edit->tempoSequence.getTempo (0)->setBpm (60.0);
        edit->getMasterVolumePlugin()->setVolumeDb (0.0);

        return edit;
    }
}
#endif

}} // namespace tracktion { inline namespace engine
