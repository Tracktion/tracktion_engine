/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#if TRACKTION_UNIT_TESTS && TRACKTION_UNIT_TESTS_HASH

#include "../../3rd_party/doctest/tracktion_doctest.hpp"
#include <algorithm>
#include <bit>
#include <unordered_set>
#include <vector>

namespace tracktion::inline core
{

TEST_SUITE ("tracktion_core")
{
    TEST_CASE ("Hash")
    {
        SUBCASE ("Determinism and order sensitivity")
        {
            std::vector<int> a { 1, 2, 3 }, b { 3, 2, 1 };
            CHECK_EQ (hash_range (a), hash_range (a));
            CHECK_NE (hash_range (a), hash_range (b));
            CHECK_NE (hash ((size_t) 0, 1), hash ((size_t) 1, 0));
            CHECK_NE (hash ((size_t) 0, 0), (size_t) 0);
        }

        SUBCASE ("Combining structurally similar node IDs doesn't collide")
        {
            // Regression test for duplicate playback graph node IDs. Two
            // ArrangerLauncherSwitchingNodes for tracks with EditItemIDs 1010 and
            // 1022 produced the same ID once their child SummingNode IDs were
            // folded in, because the old combine was close to affine.
            constexpr size_t seed = 7653239033668669842ull;
            const auto base1010 = hash (seed, (size_t) 1010);
            const auto base1022 = hash (seed, (size_t) 1022);
            CHECK_NE (base1010, base1022);
            CHECK_NE (hash (base1010, (size_t) 173965249108218ull),
                      hash (base1022, (size_t) 173965248321758ull));
        }

        SUBCASE ("Sequential IDs folded through a graph-like chain stay unique")
        {
            // Mimics a graph where every track's node ID is built from a type
            // seed, the track's sequential EditItemID, and a child ID that is
            // itself a fold of sequential clip IDs.
            constexpr size_t trackSeed = 7653239033668669842ull;
            constexpr size_t clipSeed  = 3615177560026405684ull;
            std::unordered_set<size_t> ids;
            size_t numIDs = 0;

            for (int numTracks = 1; numTracks <= 64; ++numTracks)
            {
                for (int t = 0; t < numTracks; ++t)
                {
                    const size_t trackID = 1000 + (size_t) t * 3;
                    auto id = hash (trackSeed, trackID);

                    size_t childID = 0;

                    for (int c = 0; c < 4; ++c)
                        hash_combine (childID, hash (clipSeed, trackID + 1 + (size_t) c * (size_t) numTracks));

                    hash_combine (id, childID);
                    ids.insert (id);
                    ++numIDs;
                }
            }

            CHECK_EQ (ids.size(), numIDs);
        }

        SUBCASE ("Avalanche")
        {
            // Flipping a single input bit should change about half of the output
            // bits. The old combine changed fewer than two on average.
            double totalFlipped = 0;
            int numSamples = 0;
            int minFlipped = 64;

            for (size_t seed = 1; seed < 4000; seed += 97)
            {
                for (size_t v = 1; v < 4000; v += 89)
                {
                    for (int bit = 0; bit < 64; ++bit)
                    {
                        const auto h1 = hash (seed, v);
                        const auto h2 = hash (seed, v ^ (size_t (1) << bit));
                        const auto flipped = std::popcount (static_cast<std::uint64_t> (h1 ^ h2));
                        totalFlipped += flipped;
                        minFlipped = std::min (minFlipped, flipped);
                        ++numSamples;
                    }
                }
            }

            const auto average = totalFlipped / numSamples;
            CHECK_GT (average, 28.0);
            CHECK_LT (average, 36.0);
            CHECK_GT (minFlipped, 8);
        }
    }
}

} // namespace tracktion::inline core

#endif //TRACKTION_UNIT_TESTS_HASH
