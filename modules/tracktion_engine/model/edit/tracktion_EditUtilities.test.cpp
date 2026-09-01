/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_EDIT_UTILITIES

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine {

//==============================================================================
//==============================================================================
TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("toBitSet: returns only the tracks it was given")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = Edit::createSingleTrackEdit (engine, Edit::EditRole::forRendering);
        edit->ensureNumberOfAudioTracks (3);

        auto audioTracks = getAudioTracks (*edit);
        REQUIRE (audioTracks.size() == 3);

        const auto allTracks = getAllTracks (*edit);

        SUBCASE ("A single track sets a single bit")
        {
            const auto bits = toBitSet ({ audioTracks[0] });

            CHECK (bits.countNumberOfSetBits() == 1);
            CHECK (bits[allTracks.indexOf (audioTracks[0])]);
            CHECK (! bits[allTracks.indexOf (audioTracks[1])]);
            CHECK (! bits[allTracks.indexOf (audioTracks[2])]);
        }

        SUBCASE ("A subset sets exactly those bits")
        {
            const auto bits = toBitSet ({ audioTracks[0], audioTracks[2] });

            CHECK (bits.countNumberOfSetBits() == 2);
            CHECK (bits[allTracks.indexOf (audioTracks[0])]);
            CHECK (! bits[allTracks.indexOf (audioTracks[1])]);
            CHECK (bits[allTracks.indexOf (audioTracks[2])]);
        }

        SUBCASE ("All tracks sets every bit")
        {
            const auto bits = toBitSet (allTracks);
            CHECK (bits.countNumberOfSetBits() == allTracks.size());
        }

        SUBCASE ("An empty array gives an empty bitset")
        {
            CHECK (toBitSet ({}).isZero());
        }

        SUBCASE ("Tracks from another Edit are ignored")
        {
            auto otherEdit = Edit::createSingleTrackEdit (engine, Edit::EditRole::forRendering);
            auto otherTrack = getAudioTracks (*otherEdit)[0];

            const auto bits = toBitSet ({ audioTracks[1], otherTrack });

            CHECK (bits.countNumberOfSetBits() == 1);
            CHECK (bits[allTracks.indexOf (audioTracks[1])]);
        }

        SUBCASE ("Bit indices match Track::getIndexInEditTrackList")
        {
            for (auto t : allTracks)
            {
                const auto bits = toBitSet ({ t });

                CHECK (bits.countNumberOfSetBits() == 1);
                CHECK (bits[t->getIndexInEditTrackList()]);
            }
        }

        SUBCASE ("toTrackArray round-trips the tracks passed in")
        {
            const juce::Array<Track*> subset { audioTracks[2], audioTracks[0] };
            const auto roundTripped = toTrackArray (*edit, toBitSet (subset));

            CHECK (roundTripped.size() == subset.size());

            for (auto t : subset)
                CHECK (roundTripped.contains (t));
        }
    }
}

} // namespace tracktion::inline engine

#endif
