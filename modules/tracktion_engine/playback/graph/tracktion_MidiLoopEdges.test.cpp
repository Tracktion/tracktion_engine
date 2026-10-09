/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_PLAYBACK && ENGINE_UNIT_TESTS_CLIP_LAUNCHER

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include <tracktion_engine/utilities/tracktion_TestUtilities.h>

namespace tracktion::inline engine
{

//==============================================================================
// Arrangement MIDI clips played around the arrangement loop's ends, where a
// note can be stopped as soon as it's struck or struck in a section too short
// for it, both of which used to leave notes on or not play them.
// Each is checked with a clip played from a looped sequence (the default) and
// one played from beats (MidiClip::setUsesProxy (false)).
//==============================================================================
namespace midi_loop_edge_tests
{
    using namespace clip_launcher_test_utilities;

    struct LoopedMidiEdit
    {
        LoopedMidiEdit (Engine& engine, double bpm)
            : player (engine, getPlayerParams()),
              edit (test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing))
        {
            edit->tempoSequence.getTempo (0)->set (0_bp, bpm, 1.0f, false);
            edit->tempoSequence.updateTempoData();
            probe = &addMidiProbePlugin (*getAudioTracks (*edit)[0]);
        }

        TimePosition toTime (double beat) const     { return edit->tempoSequence.toTime (BeatPosition::fromBeats (beat)); }

        MidiClip& addClip (TimeRange time, bool beatBased)
        {
            auto clip = insertMIDIClip (*getAudioTracks (*edit)[0], time);
            clip->setUsesProxy (! beatBased);
            return *clip;
        }

        void playLooped (TimeRange loop, TimePosition start, TimeDuration duration)
        {
            auto& transport = edit->getTransport();
            transport.setLoopRange (loop);
            transport.looping = true;
            transport.setPosition (start);
            transport.play (false);
            player.process (toSamples (duration, sampleRate));
        }

        int countNoteOns (int noteNumber) const
        {
            return probe->countEvents (true, noteNumber, { 0_tp, Edit::getMaximumEditEnd() });
        }

        /** Returns how a note was left, and checks it was never struck whilst it was already on. */
        bool isLeftOn (int noteNumber) const
        {
            bool isOn = false;

            for (auto& e : probe->events)
            {
                if (e.noteNumber != noteNumber)
                    continue;

                CHECK_MESSAGE (! (e.isNoteOn && isOn), ("struck while on at " + juce::String (e.outputTime, 5) + "s").toStdString());
                isOn = e.isNoteOn;
            }

            return isOn;
        }

        test_utilities::EnginePlayer player;
        std::unique_ptr<Edit> edit;
        MidiProbePlugin* probe = nullptr;
    };
}

//==============================================================================
//==============================================================================
TEST_SUITE ("tracktion_engine")
{
    using namespace midi_loop_edge_tests;

    TEST_CASE ("MIDI: a note starting on the last sample of the loop isn't left on")
    {
        auto& engine = *Engine::getEngines()[0];

        for (bool beatBased : { false, true })
        {
            CAPTURE (beatBased);
            LoopedMidiEdit e (engine, 60.0);

            // At 60bpm the loop's 4 beats are 176400 samples, so this starts in the last one, after
            // its start, which is where the notes playing are stopped as the loop ends
            auto& clip = e.addClip ({ 0_tp, 8_tp }, beatBased);
            clip.getSequence().addNote (60, BeatPosition::fromBeats (4.0 - 0.75 / sampleRate), 1_bd, 100, 0, nullptr);

            e.playLooped ({ 0_tp, 4_tp }, 0_tp, 10s);
            CHECK (! e.isLeftOn (60));
        }
    }

    TEST_CASE ("MIDI: a note held past a clip's end isn't struck when the loop starts just before its end")
    {
        auto& engine = *Engine::getEngines()[0];

        for (bool beatBased : { false, true })
        {
            CAPTURE (beatBased);

            // At 61bpm, beat 8 is 347016.39 samples, so the loop starts a fraction of a sample before
            // it, which is the last of the clip
            LoopedMidiEdit e (engine, 61.0);
            REQUIRE (toSamples (e.toTime (8.0), sampleRate) < e.toTime (8.0).inSeconds() * sampleRate);

            auto& clip = e.addClip ({ 0_tp, e.toTime (8.0) }, beatBased);
            clip.getSequence().addNote (60, 6_bp, 4_bd, 100, 0, nullptr);

            e.playLooped ({ e.toTime (8.0), e.toTime (12.0) }, e.toTime (11.0), 10s);
            CHECK (e.countNoteOns (60) == 0);
            CHECK (! e.isLeftOn (60));
        }
    }

    TEST_CASE ("MIDI: notes held over the loop start are struck again however soon the loop wraps in a block")
    {
        auto& engine = *Engine::getEngines()[0];

        for (bool beatBased : { false, true })
        {
            CAPTURE (beatBased);
            LoopedMidiEdit e (engine, 60.0);

            // A note from before the loop to after its start
            auto& clip = e.addClip ({ 0_tp, 8_tp }, beatBased);
            clip.getSequence().addNote (60, 0.5_bp, 1_bd, 100, 0, nullptr);

            // A loop 511 samples more than a whole number of blocks long, so after it wraps the
            // next block's first section is a sample long, played from the loop's start
            const auto loopLength = TimeDuration::fromSamples (blockSize * 20 + 511, sampleRate);
            e.playLooped ({ 1_tp, 1_tp + loopLength }, 1_tp, loopLength * 4.5);

            // Struck as playback starts and again each of the four times the loop wraps
            CHECK (e.countNoteOns (60) == 5);
            e.isLeftOn (60);
        }
    }
}

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_PLAYBACK && ENGINE_UNIT_TESTS_CLIP_LAUNCHER
