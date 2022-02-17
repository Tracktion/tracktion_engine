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
    inline std::unique_ptr<Edit> createTestEdit (Engine& engine)
    {
        // Make tempo 60bpm and 0dB master vol for easy calculations
        auto edit = Edit::createSingleTrackEdit (engine);
        edit->tempoSequence.getTempo (0)->setBpm (60.0);
        edit->getMasterVolumePlugin()->setVolumeDb (0.0);

        return edit;
    }
}
#endif

}} // namespace tracktion { inline namespace engine
